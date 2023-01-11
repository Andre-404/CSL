#include "heapObject.h"
#include "garbageCollector.h"


namespace memory {
	void* __allocObj(uInt64 size) {
		//only 1 thread my allocated memory in the gc
		std::scoped_lock<decltype(gc.mtx)> lk(gc.mtx);
		return gc.alloc(size);
	}
}