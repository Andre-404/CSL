#include "objects.h"

using namespace object;

//FNV-1a hash
static uInt64 hashString(char* str, uInt length) {
	uInt64 hash = 14695981039346656037u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t)str[i];
		hash *= 1099511628211;
	}
	return hash;
}

CSLString::CSLString(uInt64 length) {
	size = length;
	moveTo = nullptr;
	hash = 0;
	type = ObjType::STRING;
}

CSLString* CSLString::createString(char* from, uInt64 length) {
	//+1 for null terminator
	void* ptr = memory::__allocObj(sizeof(CSLString) + length + 1);
	CSLString* str = new(ptr) CSLString(length);
	memcpy(reinterpret_cast<byte*>(ptr) + sizeof(CSLString), from, length + 1);
	str->hash = hashString(from, length);
	return str;
}

void CSLString::move(byte* newAddress) {
	memmove(newAddress, this, getSize());
}
void CSLString::updateInteralPointers() {
	//nothing to update
}
uInt64 CSLString::getSize() {
	//+1 for terminator byte
	return sizeof(CSLString) + size + 1;
}
void CSLString::mark(vector<HeapObject*>& stack) {
	moveTo = this;
}

char* CSLString::getString() {
	return reinterpret_cast<byte*>(this) + sizeof(CSLString);
}

bool CSLString::compare(CSLString* other) {
	if (other->size != size) return false;
	return memcmp(getString(), other->getString(), size) == 0;
}

bool CSLString::compare(string other) {
	if (other.size() != size) return false;
	return memcmp(getString(), other.c_str(), size) == 0;
}

CSLString* CSLString::concat(CSLString* other) {
	uInt64 len = other->size + size + 1;
	char temp[1] = "";
	CSLString* finalStr = createString(temp, len);
	memcpy(finalStr->getString(), getString(), size);
	//+1 for null terminator
	memcpy(finalStr->getString() + size, other->getString(), other->size + 1);
	//compute hash from concated strings
	finalStr->hash = hashString(finalStr->getString(), len);

	return finalStr;
}