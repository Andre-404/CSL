#pragma once
#include "../Codegen/codegenDefs.h"
#include "../DataStructures/gcArray.h"
#include "../Objects/objects.h"

namespace runtime {
	string expectedType(string msg, Value val);
	class VM {
	public:
		VM(compileCore::Compiler* compiler);
		RuntimeResult execute();
	private:
		Value stack[STACK_MAX];
		Value* stackTop;
		ManagedArray<object::ObjUpval*> openUpvals;
		CallFrame frames[FRAMES_MAX];
		int frameCount;
		ManagedArray<Globalvar> globals;
		vector<File*> sourceFiles;

		//VM stuff
		byte getOp(long _ip);
		void concatenate();
		void push(Value val);
		Value pop();
		Value peek(int depth);

		void resetStack();
		RuntimeResult runtimeError(string err);

		bool callValue(Value callee, int argCount);
		bool call(object::ObjClosure* function, int argCount);


		object::ObjUpval* captureUpvalue(Value* local);
		void closeUpvalues(Value* last);

		void defineMethod(object::ObjString* name);
		bool bindMethod(object::ObjClass* klass, object::ObjString* name);
		bool invoke(object::ObjString* methodName, int argCount);
		bool invokeFromClass(object::ObjClass* klass, object::ObjString* fieldName, int argCount);
	};

}