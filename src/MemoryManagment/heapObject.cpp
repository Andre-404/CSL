#include "heapObject.h"
#include "garbageCollector.h"


namespace memory {
	void* __allocObj(size_t size) {
		return gc.alloc(size);
	}
}