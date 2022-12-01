#include "objects.h"
#include "../MemoryManagment/garbageCollector.h"

using namespace object;
using namespace memory;

#define TOMBSTONE (ObjString*)0x000001

//FNV-1a hash
static uInt64 hashString(char* str, uInt length) {
	uInt64 hash = 14695981039346656037u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t)str[i];
		hash *= 1099511628211;
	}
	return hash;
}

ObjString::ObjString(uInt64 length) {
	size = length;
	moveTo = nullptr;
	hash = 0;
	type = ObjType::STRING;
}

ObjString* ObjString::createString(char* from, uInt64 length) {
	//+1 for null terminator
	void* ptr = memory::__allocObj(sizeof(ObjString) + length + 1);
	ObjString* str = new(ptr) ObjString(length);
	memcpy(reinterpret_cast<byte*>(ptr) + sizeof(ObjString), from, length + 1);
	str->hash = hashString(from, length);
	return str;
}

void ObjString::move(byte* newAddress) {
	memmove(newAddress, this, getSize());
}
void ObjString::updateInternalPointers() {
	//nothing to update
}
uInt64 ObjString::getSize() {
	//+1 for terminator byte
	return sizeof(ObjString) + size + 1;
}
void ObjString::mark() {
	gc.markObj(this);
}

char* ObjString::getString() {
	return reinterpret_cast<char*>(this) + sizeof(ObjString);
}

bool ObjString::compare(ObjString* other) {
	if (other->size != size) return false;
	return memcmp(getString(), other->getString(), size) == 0;
}

bool ObjString::compare(string other) {
	if (other.size() != size) return false;
	return memcmp(getString(), other.c_str(), size) == 0;
}

ObjString* ObjString::concat(ObjString* other) {
	uInt64 len = other->size + size + 1;
	char temp[1] = "";
	ObjString* finalStr = createString(temp, len);
	memcpy(finalStr->getString(), getString(), size);
	//+1 for null terminator
	memcpy(finalStr->getString() + size, other->getString(), other->size + 1);
	//compute hash from concated strings
	finalStr->hash = hashString(finalStr->getString(), len);

	return finalStr;
}


#pragma region ObjFunction
ObjFunc::ObjFunc() {
	arity = 0;
	upvalueCount = 0;
	type = ObjType::FUNC;
	moveTo = nullptr;
	name = nullptr;
}

void ObjFunc::move(byte* to) {
	memmove(to, this, sizeof(ObjFunc));
}

void ObjFunc::mark() {
	gc.markObj(name);
	for (int i = 0; i < body.constants.size(); i++) {
		Value& val = body.constants[i];
		val.mark();
	}
	body.constants.mark();
	body.code.mark();
	body.lines.mark();
}

void ObjFunc::updateInternalPointers() {
	if (name != nullptr) name = (ObjString*)name->moveTo;
	int size = body.constants.size();
	for (int i = 0; i < size; i++) {
		body.constants[i].updatePtr();
	}
	body.constants.updateInternalPtr();
	body.code.updateInternalPtr();
	body.lines.updateInternalPtr();
}
#pragma endregion


#pragma region objNativeFn
ObjNativeFunc::ObjNativeFunc(NativeFn _func, byte _arity) {
	func = _func;
	arity = _arity;
	type = ObjType::NATIVE;
	moveTo = nullptr;
}

void ObjNativeFunc::move(byte* to) {
	memmove(to, this, sizeof(ObjNativeFunc));
}

void ObjNativeFunc::updateInternalPointers() {
	//none
}

void ObjNativeFunc::mark() {
	gc.markObj(this);
}
#pragma endregion


#pragma region objClosure
ObjClosure::ObjClosure(ObjFunc* _func) {
	func = _func;
	upvals = ManagedArray<ObjUpval*>(func->upvalueCount, nullptr);
	type = ObjType::CLOSURE;
	moveTo = nullptr;
}

void ObjClosure::move(byte* to) {
	memmove(to, this, sizeof(ObjClosure));
}

void ObjClosure::updateInternalPointers() {
	func = (ObjFunc*)func->moveTo;
	long size = upvals.size();
	for (int i = 0; i < size; i++) {
		if (upvals[i] != nullptr) upvals[i] = (ObjUpval*)upvals[i]->moveTo;
	}
}

void ObjClosure::mark() {
	gc.markObj(func);
	for (int i = 0; i < upvals.size(); i++) {
		gc.markObj(upvals[i]);
	}
	upvals.mark();
}
#pragma endregion

#pragma region objUpval
ObjUpval::ObjUpval(Value* slot) {
	location = slot;//this will have to be updated when moving objUpval
	closed = Value::nil();
	isOpen = true;
	type = ObjType::UPVALUE;
	moveTo = nullptr;
}

void ObjUpval::move(byte* to) {
	memmove(to, this, sizeof(ObjUpval));
}

void ObjUpval::mark() {
	if (!isOpen) closed.mark();
}

void ObjUpval::updateInternalPointers() {
	if(!isOpen) closed.updatePtr();
}
#pragma endregion

#pragma region objArray
ObjArray::ObjArray() {
	type = ObjType::ARRAY;
	moveTo = nullptr;
	numOfHeapPtr = 0;
}

ObjArray::ObjArray(size_t size) {
	values = ManagedArray<Value>(size);
	type = ObjType::ARRAY;
	moveTo = nullptr;
	numOfHeapPtr = 0;
}

void ObjArray::move(byte* to) {
	memmove(to, this, sizeof(ObjArray));
}

void ObjArray::mark() {
	if (numOfHeapPtr > 0) {
		for (int i = 0; i < values.size(); i++) {
			values[i].mark();
		}
	}
	values.mark();
}

void ObjArray::updateInternalPointers() {
	if (numOfHeapPtr > 0) {
		for (int i = 0; i < values.size(); i++) {
			values[i].updatePtr();
		}
	}
	values.updateInternalPtr();
}
#pragma endregion

#pragma region objClass
ObjClass::ObjClass(ObjString* _name) {
	name = _name;
	type = ObjType::CLASS;
	moveTo = nullptr;
}

void ObjClass::move(byte* to) {
	memmove(to, this, sizeof(ObjClass));
}

void ObjClass::mark() {
	name->moveTo = name;
	methods.mark();
}

void ObjClass::updateInternalPointers() {
	name = (ObjString*)name->moveTo;
	methods.updateInternalPtrs();
}
#pragma endregion

#pragma region objInstance
ObjInstance::ObjInstance(ObjClass* _klass) {
	klass = _klass;
	moveTo = nullptr;
	type = ObjType::INSTANCE;
	fields = HashMap();
}

void ObjInstance::move(byte* to) {
	memmove(to, this, sizeof(ObjInstance));
}

void ObjInstance::mark() {
	fields.mark();
	gc.markObj(klass);
}

void ObjInstance::updateInternalPointers() {
	if (klass != nullptr) klass = reinterpret_cast<ObjClass*>(klass->moveTo);
	fields.updateInternalPtrs();
}
#pragma endregion

#pragma region objBoundMethod
ObjBoundMethod::ObjBoundMethod(Value _receiver, ObjClosure* _method) {
	receiver = _receiver;
	method = _method;
	type = ObjType::BOUND_METHOD;
	moveTo = nullptr;
}

void ObjBoundMethod::move(byte* to) {
	memmove(to, this, sizeof(ObjBoundMethod));
}

void ObjBoundMethod::mark() {
	receiver.mark();
	gc.markObj(method);
}

void ObjBoundMethod::updateInternalPointers() {
	method = (ObjClosure*)method->moveTo;
	receiver.updatePtr();
}
#pragma endregion

#pragma region objFile

#pragma endregion
