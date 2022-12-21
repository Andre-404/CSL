#pragma once
#include "../MemoryManagment/heapObject.h"
#include "../Codegen/codegenDefs.h"
#include "../DataStructures/hashMap.h"
#include "../DataStructures/gcArray.h"
#include <fstream>

namespace runtime {
	class VM;
}

namespace object {

	enum class ObjType {
		STRING,
		FUNC,
		NATIVE,
		ARRAY,
		CLOSURE,
		UPVALUE,
		CLASS,
		INSTANCE,
		BOUND_METHOD,
		THREAD,
		FILE
	};

	class Obj : public memory::HeapObject{
	public:
		ObjType type;
	};

	class ObjThread;

	//pointer to a native C++ function
	typedef bool(*NativeFn)(ObjThread* fiber, int argCount, Value* args);


	//this is a header which is followed by the bytes of the string
	class ObjString : public Obj {
	public:
		uInt64 size;
		uInt64 hash;//only computed on string creation

		ObjString(uInt64 length);

		static ObjString* createString(char* from, uInt64 length, HashMap& interned);

		void move(byte* newAddress);
		void updateInternalPointers();
		uInt64 getSize();
		void mark();

		char* getString();

		bool compare(ObjString* other);

		bool compare(string other);

		ObjString* concat(ObjString* other, HashMap& interned);
	};

	class ObjArray : public Obj {
	public:
		ManagedArray<Value> values;
		//used to decrease marking speed, if an array is filled with eg. numbers there is no need to scan it for ptrs
		uInt numOfHeapPtr;
		ObjArray();
		ObjArray(size_t size);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjArray); }
		void updateInternalPointers();
		void mark();
	};

	class ObjFunc : public Obj {
	public:
		Chunk body;
		//GC worries about the string
		ObjString* name;
		//function can have a maximum of 255 parameters
		byte arity;
		int upvalueCount;
		ObjFunc();

		void move(byte* to);
		size_t getSize() { return sizeof(ObjFunc); }
		void updateInternalPointers();
		void mark();
	};

	class ObjNativeFunc : public Obj {
	public:
		NativeFn func;
		byte arity;
		ObjNativeFunc(NativeFn _func, byte _arity);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjNativeFunc); }
		void updateInternalPointers();
		void mark();
	};

	class ObjUpval : public Obj {
	public:
		//'location' pointes either to a stack of some ObjThread or to 'closed'
		Value* location;
		Value closed;
		bool isOpen;
		ObjUpval(Value* _location);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjUpval); }
		void updateInternalPointers();
		void mark();
	};

	//multiple closures with different upvalues can point to the same function
	class ObjClosure : public Obj {
	public:
		ObjFunc* func;
		ManagedArray<ObjUpval*> upvals;
		ObjClosure(ObjFunc* _func);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjClosure); }
		void updateInternalPointers();
		void mark();
	};

	//parent classes use copy down inheritance, meaning all methods of a superclass are copied into the hash map of this class
	class ObjClass : public Obj {
	public:
		ObjString* name;
		HashMap methods;
		ObjClass(ObjString* _name);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjClass); }
		void updateInternalPointers();
		void mark();
	};

	//method bound to a specific instance of a class
	//as long as the method exists, the instance it's bound to won't be GC-ed
	class ObjBoundMethod : public Obj {
	public:
		Value receiver;
		ObjClosure* method;
		ObjBoundMethod(Value _receiver, ObjClosure* _method);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjBoundMethod); }
		void updateInternalPointers();
		void mark();
	};

	//used for instances of classes and structs, if 'klass' is null then it's a struct
	class ObjInstance : public Obj {
	public:
		ObjClass* klass;
		HashMap fields;
		ObjInstance(ObjClass* _klass);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjInstance); }
		void updateInternalPointers();
		void mark();
	};

	class ObjThread : public Obj {
	public:
		Value stack[STACK_MAX];
		Value* stackTop;
		ManagedArray<ObjUpval*> openUpvals;
		CallFrame frames[FRAMES_MAX];
		int frameCount;
		ObjClosure* codeBlock;
		 
		//execution stuff
		ThreadState state;
		//this thread is blocked until blocker has completed
		ObjThread* blocker;
		//thread priority 1-10
		int priority;

		ObjThread(ObjClosure* _codeBlock);

		void move(byte* to);
		size_t getSize() { return sizeof(ObjThread); }
		void updateInternalPointers();
		void mark();
	};

	class ObjFile {

	};

}