#pragma once
#include "heapObject.h"
#include <memory>

namespace runtime {
	class VM;
}

namespace compileTools {
	class Compiler;
}


//Lisp style mark compact garbage collector with additional non moving allocations
namespace memory {
	class GarbageCollector {
	public:
		void* alloc(uInt64 size);
		void collect(runtime::VM* vm);
		void collect(compileTools::Compiler* compiler);
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
		void markRoots(compileTools::Compiler* compiler);
		void computeCompactedAddress(byte* start);
		void updatePtrs();
		void updateRootPtrs(runtime::VM* vm);
		void updateRootPtrs(compileTools::Compiler* compiler);
		void compact(byte* start);

		void traceObj(HeapObject* obj);
	};

	extern GarbageCollector gc;
}