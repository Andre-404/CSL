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
	moveTo = nullptr;
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
	moveTo = nullptr;
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
	//nothing
}

void ObjNativeFunc::mark() {
	//nothing
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

//only updates if the upvalue is closed, no need to update an open upvalue since the value struct it points to gets
//marked and updated by ObjThread
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
	gc.markObj(name);
	methods.mark();
}

void ObjClass::updateInternalPointers() {
	name = reinterpret_cast<ObjString*>(name->moveTo);
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
	//if klass is null then this is a struct and not an instance
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

#pragma region ObjThread
ObjThread::ObjThread(ObjClosure* _codeBlock) {
	codeBlock = _codeBlock;
}

void ObjThread::move(byte* to) {
	memmove(to, this, sizeof(ObjThread));
}

void ObjThread::updateInternalPointers() {
	//only open upvalues, closed upvalues are handled by closures that contain them
	for (int i = 0; i < openUpvals.size(); i++) {
		ObjUpval* upval = openUpvals[i];
		//since position of the stack is bound to the position of ObjThread everytime the thread is moved, so is the stack
		//every open upvals 'location' must be updated to new stack position
		//we take the diff between current and next memory location, and then add the diff to 'location' field
		uInt64 diff = moveTo - this;
		upval->location += diff;
		//updates the ptr to upvalues
		if (upval != nullptr) openUpvals[i] = reinterpret_cast<ObjUpval*>(upval->moveTo);

	}
	//update pointers for all values on stack
	for (Value* i = stack; i < stackTop; i++) {
		i->updatePtr();
	}
	for (int i = 0; i < frameCount; i++) {
		CallFrame* frame = &frames[i];
		frame->closure = reinterpret_cast<ObjClosure*>(frame->closure->moveTo);
	}
	codeBlock = reinterpret_cast<ObjClosure*>(codeBlock->moveTo);
	prevThread = reinterpret_cast<ObjThread*>(prevThread->moveTo);
	openUpvals.updateInternalPtr();
}

void ObjThread::mark() {
	for (Value* i = stack; i < stackTop; i++) {
		(*i).mark();
	}
	//no need to mark the values in open upvalues since they are on the stack and get marked before
	for (int i = 0; i < openUpvals.size(); i++) {
		gc.markObj(openUpvals[i]);
	}
	for (int i = 0; i < frameCount; i++) {
		CallFrame* frame = &frames[i];
		gc.markObj(frame->closure);
	}
	gc.markObj(codeBlock);
	openUpvals.mark();
}
#pragma endregion
