#pragma once
#include "../common.h"

namespace memory {

	void* __allocObj(uInt64 size);

	class HeapObject {
	public:
		HeapObject* moveTo;

		virtual void move(byte* newAddress) = 0;
		//only updates ptr references in this object
		virtual void updateInternalPointers() = 0;
		virtual uInt64 getSize() = 0;
		virtual void mark() = 0;

		//this reroutes the new operator to take memory which the GC gives out
		void* operator new(uInt64 size) {
			return __allocObj(size);
		}
		void* operator new(uInt64 size, void* to) {
			return to;
		}
		void operator delete(void* memoryBlock) {
			delete memoryBlock;
		}
	};
}