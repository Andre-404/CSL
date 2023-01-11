#include "objects.h"
#include "../MemoryManagment/garbageCollector.h"

using namespace object;
using namespace memory;

#define TOMBSTONE (ObjString*)0x000001

#pragma region ObjString
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
	hash = 0;
	type = ObjType::STRING;
}

ObjString* ObjString::createString(char* from, uInt64 length, HashMap& interned) {
	//first we check if a interned string already exists, and if it does we return its pointer
	uInt64 hash = hashString(from, length);
	ObjString* possibleInterned = findInternedString(interned, from, length, hash);
	if (possibleInterned != nullptr) return possibleInterned;
	//+1 for null terminator
	void* ptr = memory::__allocObj(sizeof(ObjString) + length + 1);
	ObjString* str = new(ptr) ObjString(length);
	memcpy(reinterpret_cast<byte*>(ptr) + sizeof(ObjString), from, length + 1);
	str->hash = hash;
	return str;
}

ObjString* createEmptyString(uInt64 length) {
	//+1 for null terminator
	void* ptr = memory::__allocObj(sizeof(ObjString) + length + 1);
	ObjString* str = new(ptr) ObjString(length);
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
	//nothing to mark
}

string ObjString::toString() {
	return string(getString());
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

ObjString* ObjString::concat(ObjString* other, HashMap& interned) {
	uInt64 len = other->size + size + 1;
	ObjString* finalStr = createEmptyString(len);
	memcpy(finalStr->getString(), getString(), size);
	//+1 for null terminator
	memcpy(finalStr->getString() + size, other->getString(), other->size + 1);
	//compute hash from concated strings
	finalStr->hash = hashString(finalStr->getString(), len - 1);
	//a bit inefficient, but we need to have the whole string in order to check if it already exists or not
	//this is faster than doing new char[] and then deleteing it if we find the interned string
	ObjString* possibleInterned = findInternedString(interned, finalStr->getString(), finalStr->size, finalStr->hash);
	if (possibleInterned != nullptr) return possibleInterned;
	return finalStr;
}
#pragma endregion

#pragma region ObjFunction
ObjFunc::ObjFunc() {
	arity = 0;
	upvalueCount = 0;
	type = ObjType::FUNC;
	name = nullptr;
}

void ObjFunc::move(byte* to) {
	memmove(to, this, sizeof(ObjFunc));
}

void ObjFunc::mark() {
	gc.markObj(name);
	int size = body.constants.size();
	for (int i = 0; i < size; i++) {
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

string ObjFunc::toString() {
	if (!name) {
		return "<anonymous function>";
	}
	return string(name->getString());
}
#pragma endregion

#pragma region objNativeFn
ObjNativeFunc::ObjNativeFunc(NativeFn _func, byte _arity) {
	func = _func;
	arity = _arity;
	type = ObjType::NATIVE;
}

void ObjNativeFunc::move(byte* to) {
	memmove(to, this, sizeof(ObjNativeFunc));
}

void ObjNativeFunc::updateInternalPointers() {
	//nothing
}

void ObjNativeFunc::mark() {
	//nothing
}

string ObjNativeFunc::toString() {
	return "<native function>";
}
#pragma endregion

#pragma region objClosure
ObjClosure::ObjClosure(ObjFunc* _func) {
	func = _func;
	upvals = ManagedArray<ObjUpval*>(func->upvalueCount, nullptr);
	type = ObjType::CLOSURE;
}

void ObjClosure::move(byte* to) {
	memmove(to, this, sizeof(ObjClosure));
}

void ObjClosure::updateInternalPointers() {
	func = (ObjFunc*)func->moveTo;
	long size = upvals.size();
	//upvalues mark the values they refer to
	for (int i = 0; i < size; i++) {
		if (upvals[i] != nullptr) upvals[i] = reinterpret_cast<ObjUpval*>(upvals[i]->moveTo);
	}
	upvals.updateInternalPtr();
}

void ObjClosure::mark() {
	gc.markObj(func);
	for (int i = 0; i < upvals.size(); i++) {
		gc.markObj(upvals[i]);
	}
	upvals.mark();
}

string ObjClosure::toString() {
	return func->toString();
}
#pragma endregion

#pragma region objUpval
ObjUpval::ObjUpval(Value* slot) {
	location = slot;//this will have to be updated when moving objUpval
	closed = Value::nil();
	isOpen = true;
	type = ObjType::UPVALUE;
}

void ObjUpval::move(byte* to) {
	memmove(to, this, sizeof(ObjUpval));
}

//only updates if the upvalue is closed, no need to update an open upvalue since the value struct it points to gets
//marked and updated by ObjThread
void ObjUpval::mark() {
	if (!isOpen) closed.mark();
}

void ObjUpval::updateInternalPointers() {
	if(!isOpen) closed.updatePtr();
}

string ObjUpval::toString() {
	return "upvalue";
}
#pragma endregion

#pragma region objArray
ObjArray::ObjArray() {
	type = ObjType::ARRAY;
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

//small optimization: if numOfHeapPtrs is 0 then we don't even scan the array for objects
//and if there are objects we only scan until we find all objects
void ObjArray::mark() {
	int temp = 0;
	int i = 0;
	uInt64 arrSize = values.size();
	while (i < arrSize && temp < numOfHeapPtr) {
		values[i].mark();
		if(values[i].isObj()) temp++;
	}
	values.mark();
}

void ObjArray::updateInternalPointers() {
	int temp = 0;
	int i = 0;
	uInt64 arrSize = values.size();
	while (i < arrSize && temp < numOfHeapPtr) {
		values[i].updatePtr();
		if (values[i].isObj()) temp++;
	}
	values.updateInternalPtr();
}

string ObjArray::toString() {
	return "<array>";
}
#pragma endregion

#pragma region objClass
ObjClass::ObjClass(ObjString* _name) {
	name = _name;
	type = ObjType::CLASS;
}

void ObjClass::move(byte* to) {
	memmove(to, this, sizeof(ObjClass));
}

void ObjClass::mark() {
	gc.markObj(name);
	methods.mark();
}

void ObjClass::updateInternalPointers() {
	name = reinterpret_cast<ObjString*>(name->moveTo);
	methods.updateInternalPtrs();
}

string ObjClass::toString() {
	return "<class " + name->toString() + ">";
}
#pragma endregion

#pragma region objInstance
ObjInstance::ObjInstance(ObjClass* _klass) {
	klass = _klass;
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
	//if klass is null then this is a struct and not an instance
	if (klass != nullptr) klass = reinterpret_cast<ObjClass*>(klass->moveTo);
	fields.updateInternalPtrs();
}

string ObjInstance::toString() {
	if (!klass) return "<struct>";
	return "<" + klass->name->toString() + " instance>";
}
#pragma endregion

#pragma region objBoundMethod
ObjBoundMethod::ObjBoundMethod(Value _receiver, ObjClosure* _method) {
	receiver = _receiver;
	method = _method;
	type = ObjType::BOUND_METHOD;
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

string ObjBoundMethod::toString() {
	return method->func->toString();
}
#pragma endregion

#pragma region ObjMutex
ObjMutex::ObjMutex() {
	mtx = new std::shared_mutex;
	type = ObjType::MUTEX;
}
ObjMutex::~ObjMutex() {
	delete mtx;
}

void ObjMutex::move(byte* to) {
	memmove(to, this, sizeof(ObjMutex));
}
void ObjMutex::updateInternalPointers() {
	//nothing
}
void ObjMutex::mark() {
	//nothing
}
string ObjMutex::toString() {
	return "<mutex>";
}
#pragma endregion

#pragma region ObjPromise
ObjPromise::ObjPromise(runtime::VM* _vm) {
	vm = _vm;
	type = ObjType::PROMISE;
}
ObjPromise::~ObjPromise() {
	if(vm) delete vm;
}

void ObjPromise::move(byte* to) {
	memmove(to, this, sizeof(ObjPromise));
}
void ObjPromise::updateInternalPointers() {
	//nothing
}
void ObjPromise::mark() {
	//nothing
}
string ObjPromise::toString() {
	return "<promise>";
}
#pragma endregion

