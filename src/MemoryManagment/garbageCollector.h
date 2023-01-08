#pragma once
#include "heapObject.h"
#include <memory>

namespace runtime {
	class VM;
}

namespace compileCore {
	class Compiler;
}


//Lisp style mark compact garbage collector with additional non moving allocations
namespace memory {
	class GarbageCollector {
	public:
		void* alloc(uInt64 size);
		void collect(runtime::VM* vm);
		void collect(compileCore::Compiler* compiler);
		void markObj(HeapObject* obj);
		GarbageCollector();
	private:
		std::unique_ptr<byte> memoryBlock;
		byte* heapTop;
		uInt64 memoryBlockSize;

		bool shouldCompact;
		//reset after each heap collection, calculated in 'markObj'
		//used before 'computeCompactedAddress' to allocated a new heap
		uInt64 shrinkedHeapSize;
		//static allocations that get transfered to heap at next 'collect'
		vector<HeapObject*> tempAllocs;

		vector<HeapObject*> markStack;

		void mark();
		void markRoots(runtime::VM* vm);
		void markRoots(compileCore::Compiler* compiler);
		void computeCompactedAddress(byte* start);
		void updatePtrs();
		void updateRootPtrs(runtime::VM* vm);
		void updateRootPtrs(compileCore::Compiler* compiler);
		void compact(byte* start);
	};

	extern GarbageCollector gc;
}