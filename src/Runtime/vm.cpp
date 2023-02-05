#include "vm.h"
#include "../Codegen/compiler.h"
#include "../MemoryManagment/garbageCollector.h"
#include "../DebugPrinting/BytecodePrinter.h"
#include <iostream>
#include <format>

using std::get;

runtime::VM::VM(compileCore::Compiler* compiler) {
	globals = compiler->globals;
	sourceFiles = compiler->sourceFiles;
	threadsPauseFlag.store(false);
	Value val = Value(new object::ObjClosure(compiler->endFuncDecl()));
	mainThread = new Thread(this);
	mainThread->startThread(&val, 1);
}

void runtime::VM::mark(memory::GarbageCollector* gc) {
	for (Globalvar& var : globals) var.val.mark();
	for (Thread* t : childThreads) t->mark(gc);
	mainThread->mark(gc);
}

void runtime::VM::execute() {
	mainThread->executeBytecode(nullptr);
}

bool runtime::VM::allThreadsPaused() {
	return false;
}

