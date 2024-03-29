#pragma once
#include "../codegen/codegenDefs.h"
#include "../MemoryManagment/garbageCollector.h"
#include "../Includes/robin_hood.h"
#include <fstream>
#include <stdio.h>
#include <shared_mutex>
#include <future>

namespace runtime {
	class VM;
	class Thread;
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
		FUTURE
	};

	class Obj {
	public:
		ObjType type;
		bool marked;

		virtual string toString() = 0;
		virtual void trace() = 0;
		virtual uInt64 getSize() = 0;
		virtual ~Obj() {};

		//this reroutes the new operator to take memory which the GC gives out
		void* operator new(uInt64 size) {
			return memory::gc.alloc(size);
		}
	};

	//pointer to a native C++ function
	using NativeFn = bool(*)(runtime::Thread* vm, int argCount, Value* args);


	//this is a header which is followed by the bytes of the string
	class ObjString : public Obj {
	public:
		string str;

		ObjString(string& str);
		~ObjString() {}

		bool compare(ObjString* other);

		bool compare(string other);

		ObjString* concat(ObjString* other);

		void trace();
		string toString();
		uInt64 getSize();
	};

	class ObjArray : public Obj {
	public:
		vector<Value> values;
		//used to decrease marking speed, if an array is filled with eg. numbers there is no need to scan it for ptrs
		uInt numOfHeapPtr;
		ObjArray();
		ObjArray(size_t size);
		~ObjArray() {}

		void trace();
		string toString();
		uInt64 getSize();
	};

	class ObjFunc : public Obj {
	public:
		uInt64 bytecodeOffset;
		uInt64 constantsOffset;
		string name;
		//function can have a maximum of 255 parameters
		byte arity;
		int upvalueCount;
		ObjFunc();
		~ObjFunc() {}

		void trace();
		string toString();
		uInt64 getSize();
	};

	class ObjNativeFunc : public Obj {
	public:
		NativeFn func;
		byte arity;
		ObjNativeFunc(NativeFn _func, byte _arity);
		~ObjNativeFunc() {}

		void trace();
		string toString();
		uInt64 getSize();
	};

	class ObjUpval : public Obj {
	public:
		Value val;
		ObjUpval(Value& _value);
		~ObjUpval() {}

		void trace();
		string toString();
		uInt64 getSize();
	};

	//multiple closures with different upvalues can point to the same function
	class ObjClosure : public Obj {
	public:
		ObjFunc* func;
		vector<ObjUpval*> upvals;
		ObjClosure(ObjFunc* _func);
		~ObjClosure() {}

		void trace();
		string toString();
		uInt64 getSize();
	};

	//parent classes use copy down inheritance, meaning all methods of a superclass are copied into the hash map of this class
	class ObjClass : public Obj {
	public:
		string name;
		robin_hood::unordered_node_map<string, Value> methods;
		ObjClass(string _name);
		~ObjClass() {}

		void trace();
		string toString();
		uInt64 getSize();
	};

	//method bound to a specific instance of a class
	//as long as the method exists, the instance it's bound to won't be GC-ed
	class ObjBoundMethod : public Obj {
	public:
		Value receiver;
		ObjClosure* method;
		ObjBoundMethod(Value _receiver, ObjClosure* _method);
		~ObjBoundMethod() = default;

		void trace();
		string toString();
		uInt64 getSize();
	};

	//used for instances of classes and structs, if 'klass' is null then it's a struct
	class ObjInstance : public Obj {
	public:
		ObjClass* klass;
		robin_hood::unordered_map<string, Value> fields;
		ObjInstance(ObjClass* _klass);
		~ObjInstance() = default;

		void trace();
		string toString();
		uInt64 getSize();
	};

	class ObjFile : public Obj {
	public:
		std::fstream stream;
		string path;

		ObjFile(string& path);
		~ObjFile();

		void trace();
		string toString();
		uInt64 getSize();
	};

	//language representation of a mutex object
	class ObjMutex : public Obj {
		std::shared_mutex mtx;

		ObjMutex();
		~ObjMutex();

		void trace();
		string toString();
		uInt64 getSize();
	};

	//returned by "async func()" call, when the thread finishes it will populate returnVal and delete the vm
	class ObjFuture : public Obj {
	public:
		std::future<void> fut;
		Value val;
		runtime::Thread* thread;

		ObjFuture(runtime::Thread* t);
		~ObjFuture();

		void startParallelExecution();

		void trace();
		string toString();
		uInt64 getSize();
	};
}