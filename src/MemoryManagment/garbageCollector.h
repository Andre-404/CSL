#pragma once
#include "heapObject.h"

//Lisp style mark compact garbage collector with additional non moving allocations
namespace memory {
	class GarbageCollector {
	public:
		void* alloc(uInt64 size);
		void collect();
	private:
		byte* memoryBlock;
		byte* stackTop;
		uInt64 memoryBlockSize;

		vector<HeapObject*> tempAllocs;
		uInt64 tempAllocsSize;

	};

	extern GarbageCollector gc;
}