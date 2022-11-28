#include "garbageCollector.h"
#include "../ErrorHandling/errorHandler.h"
#include <format>
#include <immintrin.h>

//start size of heap in KB
#define HEAP_START_SIZE 1024
//percentage of total heap size
#define HEAP_COMPACT_LIMIT 0.9

using std::unique_ptr;

namespace memory {
	GarbageCollector gc = GarbageCollector();

	GarbageCollector::GarbageCollector() {
		memoryBlock = unique_ptr<byte>(new byte[1024*HEAP_START_SIZE]);
		heapTop = memoryBlock.get();
		memoryBlockSize = 1024 * HEAP_START_SIZE;

		shouldCompact = false;
	}

	void* GarbageCollector::alloc(uInt64 size) {
		uInt64 availableSize = memoryBlockSize - (heapTop - memoryBlock.get());
		//if we have enough memory on the compacted heap
		if (availableSize > size) {
			heapTop += size;
			availableSize -= size;
			shouldCompact = ((memoryBlockSize - availableSize) / memoryBlockSize) > HEAP_COMPACT_LIMIT;
			return (heapTop - size);
		}
		else {
			//otherwise we allocate using new and put the pointer in the tempAllocs array,
			//objects in tempAllocs will be moved to the compacted heap on next compact
			byte* block = nullptr;

			try {
				block = new byte[size];
			}
			catch (const std::bad_alloc& e) {
				errorHandler::addSystemError(std::format("Failed allocation, tried to allocate {}", size));
				throw errorHandler::SystemException();
			}
			//this is fine to do since we won't be accessing 'block' ptr until after the object is initialized
			tempAllocs.push_back(reinterpret_cast<HeapObject*>(block));
			shouldCompact = true;
			return block;
		}
	}

	void GarbageCollector::collect(runtime::VM* vm) {
		uInt64 newSize = 0;
		newSize += markRoots(vm);
		newSize += mark();

		unique_ptr<byte> ptr = nullptr;
		if (newSize > memoryBlockSize * 0.9) {
			//Amortized size to reduce the number of future resizes
			memoryBlockSize = (1ll << (64 - _lzcnt_u64(newSize - 1)));

			ptr = unique_ptr<byte>(new byte[memoryBlockSize]);
		}
		byte* newMemoryBlock = ptr.get() == nullptr ? memoryBlock.get() : ptr.get();
		computeCompactedAddress(newMemoryBlock);

		updateRootPtrs(vm);
		updatePtrs();

		compact(newMemoryBlock);
		//if we allocated a new memory block, we give the ownership of that ptr to 'memoryBlock'
		//the old memory block is put into 'ptr' and released as soon as this funciton finishes executing
		if (ptr.get() != nullptr) std::swap(memoryBlock, ptr);

		for (HeapObject* obj : tempAllocs) {
			delete obj;
		}
		tempAllocs.clear();
	}

	void GarbageCollector::collect(compileTools::Compiler* compiler) {
		uInt64 newSize = 0;
		newSize += markRoots(compiler);
		newSize += mark();

		unique_ptr<byte> ptr = nullptr;
		if (newSize > memoryBlockSize * 0.9) {
			//Amortized size to reduce the number of future resizes
			memoryBlockSize = (1ll << (64 - _lzcnt_u64(newSize - 1)));

			ptr = unique_ptr<byte>(new byte[memoryBlockSize]);
		}
		byte* newMemoryBlock = ptr.get() == nullptr ? memoryBlock.get() : ptr.get();
		computeCompactedAddress(newMemoryBlock);

		updateRootPtrs(compiler);
		updatePtrs();

		compact(newMemoryBlock);
		//if we allocated a new memory block, we give the ownership of that ptr to 'memoryBlock'
		//the old memory block is put into 'ptr' and released as soon as this funciton finishes executing
		if (ptr.get() != nullptr) std::swap(memoryBlock, ptr);
	}

	uInt64 GarbageCollector::mark() {
		uInt64 size = 0;
		//we use a stack to avoid going into a deep recursion(which might fail)
		while (markStack.size() != 0) {
			HeapObject* ptr = markStack.back();
			markStack.pop_back();
			size += traceObj(ptr);
		}
		return size;
	}

	uInt64 GarbageCollector::markRoots(runtime::VM* vm) {
		return 0;
	}

	uInt64 GarbageCollector::markRoots(compileTools::Compiler* compiler) {
		return 0;
	}

	void GarbageCollector::computeCompactedAddress(byte* start) {
		byte* to = start;
		byte* from = memoryBlock.get();
		while (from < heapTop) {
			HeapObject* temp = reinterpret_cast<HeapObject*>(from);
			if (temp->moveTo) {
				temp->moveTo = reinterpret_cast<HeapObject*>(to);
				//move the compacted position pointer
				to += temp->getSize();
			}
			//get the next object from old heap
			from += temp->getSize();
		}
		for (HeapObject* temp : tempAllocs) {
			temp->moveTo = reinterpret_cast<HeapObject*>(to);
			//move the compacted position pointer
			to += temp->getSize();
		}
	}

	void GarbageCollector::updatePtrs() {
		byte* current = memoryBlock.get();
		while (current < heapTop) {
			HeapObject* temp = reinterpret_cast<HeapObject*>(current);
			if (temp->moveTo) {
				temp->updateInteralPointers();
			}
			current += temp->getSize();
		}

		for (HeapObject* curObject : tempAllocs) {
			if (curObject->moveTo) {
				curObject->updateInteralPointers();
			}
		}
	}

	void GarbageCollector::updateRootPtrs(runtime::VM* vm) {

	}

	void GarbageCollector::updateRootPtrs(compileTools::Compiler* compiler) {

	}

	void GarbageCollector::compact(byte* start) {
		byte* newStackTop = start;
		byte* from = memoryBlock.get();;
		//stackTop points to the top of the old heap, we ONLY update stackTop once we're done with compacting
		while (from < heapTop) {
			HeapObject* curObject = reinterpret_cast<HeapObject*>(from);
			size_t sizeOfObj = curObject->getSize();
			byte* nextObj = from + sizeOfObj;

			if (curObject->moveTo) {
				byte* to = reinterpret_cast<byte*>(curObject->moveTo);
				curObject->moveTo = nullptr;//reset the marked flag
				//this is a simple optimization, if the object doesn't move in memory at all, there's no need to copy it
				if (from != to) curObject->move(to);
				newStackTop = to + sizeOfObj;
			}
			else {
				curObject->~HeapObject();
			}
			from = nextObj;
		}

		for (HeapObject* curObject : tempAllocs) {
			size_t sizeOfObj = curObject->getSize();
			//temp alloc objects are almost always still alive, but just in case
			if (curObject->moveTo) {
				byte* to = reinterpret_cast<byte*>(curObject->moveTo);
				curObject->moveTo = nullptr;//reset the marked flag
				newStackTop = to + sizeOfObj;
			}
			else {
				curObject->~HeapObject();
			}
		}
		heapTop = newStackTop;
	}

	uInt64 GarbageCollector::traceObj(HeapObject* obj) {
		if (!obj || obj->moveTo != nullptr) return 0;
		obj->moveTo = obj;
		obj->mark(markStack);
		return obj->getSize();
	}
}