#include "hashMap.h"
#include "../Objects/objects.h"

#define TOMBSTONE (CSLString*)0x000001
#define TABLE_LOAD_FACTOR 0.65//arbitrary, test and see what's best

using namespace object;

HashMap::HashMap() {
	count = 0;
}

//returns true if 'key' wasn't already present in the hash table
bool HashMap::set(CSLString* key, Value val) {
	//adjust array size based on load factor, array size should be power of 2 for best performance
	if (count + 1 >= entries.size() * TABLE_LOAD_FACTOR) {
		//arbitrary value, should be tested and changed
		resize(((entries.size()) < 8 ? 8 : (entries.size()) * 2));
	}
	HashEntry* entry = findEntry(entries, key);
	//if the bucket was marked as a tombstone then it's already accounted for in 'count'
	bool isNewKey = entry->key == nullptr || entry->key == TOMBSTONE;
	if (isNewKey && entry->key == nullptr) count++;

	entry->key = key;
	entry->val = val;
	return isNewKey;
}

//returns true if we managed to find the entry and puts the value of the entry in 'val'
bool HashMap::get(CSLString* key, Value* val) {
	if (count == 0) return false;

	HashEntry* entry = findEntry(entries, key);
	if (entry->key == nullptr || entry->key == TOMBSTONE) return false;//failed to find key

	*val = entry->val;
	return true;
}

//O(n) but we rarely use it so it's fine
CSLString* HashMap::getKey(Value val) {
	for (int i = 0; i < entries.size(); i++) {
		Value& toCompare = entries[i].val;
		if (val.equals(toCompare)) return entries[i].key;
	}
	return nullptr;
}

//returns true if deletion was succesful, and false otherwise
bool HashMap::del(CSLString* key) {
	if (count == 0) return false;

	// Find the entry.
	HashEntry* entry = findEntry(entries, key);
	if (entry->key == nullptr || entry->key == TOMBSTONE) return false;

	// Place a tombstone in the entry.
	entry->key = TOMBSTONE;
	entry->val = Value::nil();
	return true;
}

void HashMap::tableAddAll(HashMap* other) {
	for (int i = 0; i < other->entries.size(); i++) {
		HashEntry* entry = &other->entries[i];
		if (entry->key != nullptr && entry->key != TOMBSTONE) {
			set(entry->key, entry->val);
		}
	}
}

void HashMap::resize(uInt64 newCapacity) {
	//create new array and fill it with 
	ManagedArray<HashEntry> newEntries = ManagedArray<HashEntry>(newCapacity);
	int oldcount = count;
	count = 0;
	//copy over the entries of the old array into the new one, avoid tombstones and recalculate the index
	for (int i = 0; i < entries.size(); i++) {
		HashEntry* entry = &entries[i];
		if (entry->key == nullptr || entry->key == TOMBSTONE) continue;

		HashEntry* dest = findEntry(newEntries, entry->key);
		dest->key = entry->key;
		dest->val = entry->val;
		count++;
	}

	entries = newEntries;
}

HashEntry* HashMap::findEntry(ManagedArray<HashEntry>& _entries, CSLString* key) {
	//instead of using modulo operator, we take the remainder of the division of the hash and table capacity
	//by &, this will always work because _entries.size() is a power of 2
	uInt64 bitMask = _entries.size() - 1;
	uInt64 index = key->hash & bitMask;
	HashEntry* tombstone = nullptr;
	//loop until we either find the key we're looking for, or a open slot(tombstone)
	while (true) {
		HashEntry* entry = &_entries[index];
		if (entry->key == nullptr) {//empty entry
			return tombstone != nullptr ? tombstone : entry;
		}
		else if (entry->key == TOMBSTONE) {
			//don't immediatelly return if tombstone is encountered, as it could just be on the path to the entry
			if (tombstone == nullptr) tombstone = entry;
		}
		else if (entry->key == key) {
			// We found the key.
			return entry;
		}
		//make sure to loop back to the start of the array if we exceed the array length
		index = (index + 1) & bitMask;
	}
}

//both the compiler and VM have a HashMap with created strings, this ensures no duplicate strings are ever created
//when a new string is created we first run it through this function and if the same string exists in the HashMap 'newStrnig' is discarded
CSLString* findInternedString(HashMap* table, CSLString* newString) {
	if (table->count == 0) return nullptr;
	uInt64 hash = newString->hash;

	size_t bitMask = table->entries.size() - 1;
	uInt64 index = hash & bitMask;
	while (true) {
		HashEntry* entry = &table->entries[index];
		if (entry->key == nullptr) {
			// Stop if we find an empty non-tombstone entry.
			return nullptr;
		}
		else if (entry->key != TOMBSTONE && entry->key->hash == hash && entry->key->compare(newString)) {
			// We found it.
			return entry->key;
		}
		//make sure to loop back to the start of the array if we exceed the array length
		index = (index + 1) & bitMask;
	}
}