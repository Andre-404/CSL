#pragma once
#include "gcArray.h"
#include "../Codegen/codegenDefs.h"

namespace object {
	class CSLString;
}

struct HashEntry {
	object::CSLString* key;
	Value val;

	HashEntry() {
		key = nullptr;
		val = Value::nil();
	}
};

class HashMap {
public:
	HashMap();
	bool set(object::CSLString* key, Value val);
	bool get(object::CSLString* key, Value* val);
	object::CSLString* getKey(Value val);
	bool del(object::CSLString* key);
	void tableAddAll(HashMap* other);

	friend object::CSLString* findInternedString(HashMap* table, object::CSLString* newString);

private:
	ManagedArray<HashEntry> entries;
	uInt64 count;

	void resize(uInt64 newCapacity);
	HashEntry* findEntry(ManagedArray<HashEntry>& _entries, object::CSLString* key);
};

