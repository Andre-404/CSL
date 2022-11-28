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
		GarbageCollector();
	private:
		std::unique_ptr<byte> memoryBlock;
		byte* heapTop;
		uInt64 memoryBlockSize;

		bool shouldCompact;

		vector<HeapObject*> tempAllocs;

		vector<HeapObject*> markStack;

		uInt64 mark();
		uInt64 markRoots(runtime::VM* vm);
		uInt64 markRoots(compileTools::Compiler* compiler);
		void computeCompactedAddress(byte* start);
		void updatePtrs();
		void updateRootPtrs(runtime::VM* vm);
		void updateRootPtrs(compileTools::Compiler* compiler);
		void compact(byte* start);

		uInt64 traceObj(HeapObject* obj);
	};

	extern GarbageCollector gc;
}