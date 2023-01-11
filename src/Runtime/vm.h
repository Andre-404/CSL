#pragma once
#include "../Codegen/codegenDefs.h"
#include "../DataStructures/gcArray.h"
#include "../Objects/objects.h"


class VM {
	Value stack[STACK_MAX];
	Value* stackTop;
	ManagedArray<object::ObjUpval*> openUpvals;
	CallFrame frames[FRAMES_MAX];
	int frameCount;
	object::ObjClosure* codeBlock;
};