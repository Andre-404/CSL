#pragma once
#include "gcArray.h"
#include "../Codegen/codegenDefs.h"

namespace object {
	class ObjString;
}


namespace memory {
	class HeapObject;
}

struct HashEntry {
	object::ObjString* key;
	Value val;

	HashEntry() {
		key = nullptr;
		val = Value::nil();
	}
};

class HashMap {
public:
	HashMap();
	bool set(object::ObjString* key, Value val);
	bool get(object::ObjString* key, Value* val);
	object::ObjString* getKey(Value val);
	bool del(object::ObjString* key);
	void tableAddAll(HashMap* other);

	friend object::ObjString* findInternedString(HashMap* table, object::ObjString* newString);

	void mark();
	void updateInternalPtrs();
private:
	ManagedArray<HashEntry> entries;
	uInt64 count;

	void resize(uInt64 newCapacity);
	HashEntry* findEntry(ManagedArray<HashEntry>& _entries, object::ObjString* key);
};

