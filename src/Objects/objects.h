#pragma once
#include "../MemoryManagment/heapObject.h"
#include "../Codegen/codegenDefs.h"
#include "../DataStructures/hashMap.h"
#include "../DataStructures/gcArray.h"
#include <fstream>
#include <stdio.h>
#include <shared_mutex>

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
		FILE,
		MUTEX,
		PROMISE
	};

	class Obj : public memory::HeapObject{
	public:
		ObjType type;

		virtual string toString() = 0;
	};

	//pointer to a native C++ function
	using NativeFn = bool(*)(runtime::VM* vm, int argCount, Value* args);


	//this is a header which is followed by the bytes of the string
	class ObjString : public Obj {
	public:
		uInt64 size;
		uInt64 hash;//only computed on string creation

		ObjString(uInt64 length);
		~ObjString() {}

		static ObjString* createString(char* from, uInt64 length, HashMap& interned);

		char* getString();

		bool compare(ObjString* other);

		bool compare(string other);

		ObjString* concat(ObjString* other, HashMap& interned);

		void move(byte* newAddress);
		void updateInternalPointers();
		uInt64 getSize();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjString"; }
		#endif
	};

	class ObjArray : public Obj {
	public:
		ManagedArray<Value> values;
		//used to decrease marking speed, if an array is filled with eg. numbers there is no need to scan it for ptrs
		uInt numOfHeapPtr;
		ObjArray();
		ObjArray(size_t size);
		~ObjArray() {}

		void move(byte* to);
		size_t getSize() { return sizeof(ObjArray); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjArray"; }
		#endif
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
		~ObjFunc() {}

		void move(byte* to);
		size_t getSize() { return sizeof(ObjFunc); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjFunc"; }
		#endif
	};

	class ObjNativeFunc : public Obj {
	public:
		NativeFn func;
		byte arity;
		ObjNativeFunc(NativeFn _func, byte _arity);
		~ObjNativeFunc() {}

		void move(byte* to);
		size_t getSize() { return sizeof(ObjNativeFunc); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjNativeFunc"; }
		#endif
	};

	class ObjUpval : public Obj {
	public:
		//'location' pointes either to a stack of some ObjThread or to 'closed'
		Value* location;
		Value closed;
		bool isOpen;
		ObjUpval(Value* _location);
		~ObjUpval() {}

		void move(byte* to);
		size_t getSize() { return sizeof(ObjUpval); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjUpval"; }
		#endif
	};

	//multiple closures with different upvalues can point to the same function
	class ObjClosure : public Obj {
	public:
		ObjFunc* func;
		ManagedArray<ObjUpval*> upvals;
		ObjClosure(ObjFunc* _func);
		~ObjClosure() {}

		void move(byte* to);
		size_t getSize() { return sizeof(ObjClosure); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjClosure"; }
		#endif
	};

	//parent classes use copy down inheritance, meaning all methods of a superclass are copied into the hash map of this class
	class ObjClass : public Obj {
	public:
		ObjString* name;
		HashMap methods;
		ObjClass(ObjString* _name);
		~ObjClass() {}

		void move(byte* to);
		size_t getSize() { return sizeof(ObjClass); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjClass"; }
		#endif
	};

	//method bound to a specific instance of a class
	//as long as the method exists, the instance it's bound to won't be GC-ed
	class ObjBoundMethod : public Obj {
	public:
		Value receiver;
		ObjClosure* method;
		ObjBoundMethod(Value _receiver, ObjClosure* _method);
		~ObjBoundMethod() {}

		void move(byte* to);
		size_t getSize() { return sizeof(ObjBoundMethod); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjBoundMethod"; }
		#endif
	};

	//used for instances of classes and structs, if 'klass' is null then it's a struct
	class ObjInstance : public Obj {
	public:
		ObjClass* klass;
		HashMap fields;
		ObjInstance(ObjClass* _klass);
		~ObjInstance() {};

		void move(byte* to);
		size_t getSize() { return sizeof(ObjInstance); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjInstance"; }
		#endif
	};

	//using C-style file accessing since the reference to the file is a pointer rather than a file stream object
	//this is done because stream objects can't be copied/moved, and all Obj objects are moved in memory on the managed heap
	class ObjFile : public Obj {
	public:
		FILE* file;
		ObjString* path;

		ObjFile(FILE* file);
		~ObjFile();

		void move(byte* to);
		size_t getSize() { return sizeof(ObjFile); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjFile"; }
		#endif
	};

	//language representation of a mutex object
	class ObjMutex : public Obj {
		std::shared_mutex* mtx;

		ObjMutex();
		~ObjMutex();

		void move(byte* to);
		size_t getSize() { return sizeof(ObjMutex); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjMutex"; }
		#endif
	};

	//returned by "async func()" call, when the thread finishes it will populate returnVal and delete the vm
	class ObjPromise : public Obj {
		runtime::VM* vm;
		Value returnVal;

		ObjPromise(runtime::VM* vm);
		~ObjPromise();

		void move(byte* to);
		size_t getSize() { return sizeof(ObjPromise); }
		void updateInternalPointers();
		void mark();
		string toString();
		#ifdef GC_PRINT_HEAP
		string gcDebugToStr() { return "ObjPromise"; }
		#endif
	};
}