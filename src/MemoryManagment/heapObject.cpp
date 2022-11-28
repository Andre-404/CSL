#include "heapObject.h"
#include "garbageCollector.h"


namespace memory {
	void* __allocObj(uInt64 size) {
		return gc.alloc(size);
	}
}