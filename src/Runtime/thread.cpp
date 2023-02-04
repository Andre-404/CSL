#include "thread.h"
#include "vm.h"
#include "../MemoryManagment/garbageCollector.h"
#include "../DebugPrinting/BytecodePrinter.h"
#include <iostream>
#include <format>

using std::get;

runtime::Thread::Thread(VM* _vm){
	stackTop = stack;
	frameCount = 0;
	paused.store(false);
	vm = _vm;
}
// Copies the callee and all arguments, otherStack points to the callee, arguments are on top of it on the stack
void runtime::Thread::startThread(Value* otherStack, int num) {
	memcpy(stackTop, otherStack, sizeof(Value) * num);
	stackTop += num;
	callValue(*otherStack, num - 1);
}


#pragma region Helpers
void runtime::Thread::mark(memory::GarbageCollector* gc) {
	for (Value* i = stack; i < stackTop; i++) {
		i->mark();
	}
	for (int i = 0; i < frameCount; i++) gc->markObj(frames[i].closure);
}

string runtime::expectedType(string msg, Value val) {
	return msg + val.typeToStr() + ".";
}

byte runtime::Thread::getOp(long _ip) {
	return frames[frameCount - 1].closure->func->body.code[_ip];
}

void runtime::Thread::push(Value val) {
	if (stackTop >= stack + STACK_MAX) {
		runtimeError("Stack overflow");
		exit(64);
	}
	*stackTop = val;
	stackTop++;
}

Value runtime::Thread::pop() {
	stackTop--;
	return *stackTop;
}

Value runtime::Thread::peek(int depth) {
	return stackTop[-1 - depth];
}

void runtime::Thread::runtimeError(string err) {
	const string cyan = "\u001b[38;5;117m";
	const string black = "\u001b[0m";
	const string red = "\u001b[38;5;196m";
	const string yellow = "\u001b[38;5;220m";

	std::cout << red + "runtime error: \n" + black + err + "\n";
	//prints callstack
	for (int i = frameCount - 1; i >= 0; i--) {
		CallFrame* frame = &frames[i];
		object::ObjFunc* function = frame->closure->func;
		uInt64 instruction = frame->ip - 1;
		codeLine line = function->body.getLine(instruction);
		//fileName:line | in <func name>()
		string temp = yellow + line.getFileName(vm->sourceFiles) + black + ":" + cyan + std::to_string(line.line + 1) + " | " + black;
		std::cout << temp << "in ";
		std::cout << (function->name.length() == 0 ? "script" : function->name) << "()\n";
	}
	exit(64);
}

static bool isFalsey(Value value) {
	return ((value.isBool() && !get<bool>(value.value)) || value.isNil());
}

bool runtime::Thread::callValue(Value callee, int argCount) {
	if (callee.isObj()) {
		switch (get<object::Obj*>(callee.value)->type) {
		case object::ObjType::CLOSURE:
			return call(callee.asClosure(), argCount);
		case object::ObjType::NATIVE: {
			int arity = callee.asNativeFn()->arity;
			//if arity is -1 it means that the function takes in a variable number of args
			if (arity != -1 && argCount != arity) {
				runtimeError(std::format("Expected {} arguments for function call but got {}.", arity, argCount));
				return false;
			}
			object::NativeFn native = callee.asNativeFn()->func;
			//native functions throw strings when a error has occured
			try {
				//fiber ptr is passes because a native function might create a new callstack or mutate the stack
				bool shouldPop = native(this, argCount, stackTop - argCount);
				//shouldPop is false if the native function already popped it's arguments(eg. if a native created a new callframe)
				if (shouldPop) {
					//right now the result of the native function sits above the arguments, so we first take the result
					Value top = pop();
					//pop the args + native function itself
					stackTop -= argCount + 1;
					//push the result of the native function back on top
					push(top);
				}
			}
			catch (string str) {
				//globals are guaranteed not to change after the native funcs have been defined
				if (str == "") return false;
				runtimeError(std::format("Error: {}", str));
				return false;
			}
			//native functions take care of pushing results themselves, so we just return true
			return true;
		}
		case object::ObjType::CLASS: {
			//we do this so if a GC runs we safely update all the pointers(since the stack is considered a root)
			push(callee);
			stackTop[-argCount - 2] = Value(new object::ObjInstance(peek(0).asClass()));
			object::ObjClass* klass = pop().asClass();
			auto it = klass->methods.find(klass->name);
			if (it != klass->methods.end()) {
				return call(it->second.asClosure(), argCount);
			}
			else if (argCount != 0) {
				runtimeError(std::format("Class constructor expects 0 arguments but got {}.", argCount));
				return false;
			}
			return true;
		}
		case object::ObjType::BOUND_METHOD: {
			//puts the receiver instance in the 0th slot of the current callframe('this' points to the 0th slot)
			object::ObjBoundMethod* bound = callee.asBoundMethod();
			stackTop[-argCount - 1] = bound->receiver;
			return call(bound->method, argCount);
		}
		default:
			break; // Non-callable object type.
		}
	}
	runtimeError("Can only call functions and classes.");
	return false;
}

bool runtime::Thread::call(object::ObjClosure* closure, int argCount) {
	if (argCount != closure->func->arity) {
		runtimeError(std::format("Expected {} arguments for function call but got {}.", closure->func->arity, argCount));
		return false;
	}

	if (frameCount == FRAMES_MAX) {
		runtimeError("Stack overflow.");
		return false;
	}

	CallFrame* frame = &frames[frameCount++];
	frame->closure = closure;
	frame->ip = 0;
	frame->slots = stackTop - argCount - 1;
	return true;

}

object::ObjUpval* runtime::Thread::captureUpvalue(Value* local) {
	object::ObjUpval* upval = new object::ObjUpval(*local);
	*local = Value(upval);
}

void runtime::Thread::defineMethod(string& name) {
	//no need to typecheck since the compiler made sure to emit code in this order
	Value method = peek(0);
	object::ObjClass* klass = peek(1).asClass();
	klass->methods.insert_or_assign(name, method);
	//we only pop the method, since other methods we're compiling will also need to know their class
	pop();
}

bool runtime::Thread::bindMethod(object::ObjClass* klass, string& name) {
	//At the start the instance whose method we're binding needs to be on top of the stack
	Value method;
	auto it = klass->methods.find(klass->name);
	if (it == klass->methods.end()) {
		runtimeError(std::format("{} doesn't contain method '{}'.", klass->name, name));
		return false;
	}
	//we need to push the method to the stack because if a GC collection happens the ptr inside of method becomes invalid
	//easiest way for it to update is to push it onto the stack
	push(method);
	//peek(1) because the method(closure) itself is on top of the stack right now
	object::ObjBoundMethod* bound = new object::ObjBoundMethod(peek(1), nullptr);
	//make sure to pop the closure to maintain stack discipline and to get the updated value(if it changed)
	method = pop();
	bound->method = method.asClosure();
	//pop the receiver instance
	pop();
	push(Value(bound));
	return true;
}

bool runtime::Thread::invoke(string& fieldName, int argCount) {
	Value receiver = peek(argCount);
	if (!receiver.isInstance()) {
		runtimeError(std::format("Only instances can call methods, got {}.", receiver.typeToStr()));
		return false;
	}

	object::ObjInstance* instance = receiver.asInstance();
	auto it = instance->fields.find(fieldName);
	if (it != instance->fields.end()) {
		stackTop[-argCount - 1] = it->second;
		return callValue(it->second, argCount);
	}
	//this check is used because we also use objInstance to represent struct literals
	//and if this instance is a struct it can only contain functions inside it's field table
	if (instance->klass == nullptr) {
		runtimeError(std::format("Undefined property '{}'.", fieldName));
		return false;
	}

	return invokeFromClass(instance->klass, fieldName, argCount);
}

bool runtime::Thread::invokeFromClass(object::ObjClass* klass, string& methodName, int argCount) {
	auto it = klass->methods.find(methodName);
	if (it == klass->methods.end()) {
		runtimeError(std::format("Class '{}' doesn't contain '{}'.", klass->name, methodName));
		return false;
	}
	//the bottom of the call stack will contain the receiver instance
	return call(it->second.asClosure(), argCount);
}
#pragma endregion

void runtime::Thread::executeBytecode(object::ObjFuture* fut) {
	CallFrame* frame = &frames[frameCount - 1];
	#pragma region Helpers and Macros

	auto readByte = [&]() { return getOp(frame->ip++); };
	auto readShort = [&]() { return frame->ip += 2, (uint16_t)((getOp(frame->ip - 2) << 8) | getOp(frame->ip - 1)); };
	auto readConstant = [&]() { return frames[frameCount - 1].closure->func->body.constants[readByte()]; };
	auto readConstantLong = [&]() { return frames[frameCount - 1].closure->func->body.constants[readShort()]; };
	auto readString = [&]() { return readConstant().asString(); };
	auto readStringLong = [&]() { return readConstantLong().asString(); };
	auto checkArrayBounds = [&](Value field, Value callee) {
		if (!field.isNumber()) runtimeError(std::format("Index must be a number, got {}.", callee.typeToStr()));
		double index = get<double>(field.value);
		object::ObjArray* arr = callee.asArray();
		//Trying to access a variable using a float is a error
		if (!IS_INT(index)) runtimeError("Expected interger, got float.");
		if (index < 0 || index > arr->values.size() - 1)
			runtimeError(std::format("Index {} outside of range [0, {}].", (uInt64)index, callee.asArray()->values.size() - 1));
		return index;
	};

	#define BINARY_OP(valueType, op) \
		do { \
			if (!peek(0).isNumber() || !peek(1).isNumber()) { \
				runtimeError(std::format("Operands must be numbers, got '{}' and '{}'.", peek(1).typeToStr(), peek(0).typeToStr())); \
			} \
			double b = get<double>(pop().value); \
			double a = get<double>(pop().value); \
			push(Value(a op b)); \
		} while (false)

	#define INT_BINARY_OP(valueType, op)\
		do {\
			if (!peek(0).isNumber() || !peek(1).isNumber()) { \
				runtimeError(std::format("Operands must be numbers, got '{}' and '{}'.", peek(1).typeToStr(), peek(0).typeToStr())); \
			} \
			double b = get<double>(pop().value); \
			double a = get<double>(pop().value); \
			push(Value((double)((uInt64)a op (uInt64)b))); \
		} while (false)

	#ifdef DEBUG_TRACE_EXECUTION
	std::cout << "-------------Code execution starts-------------\n";
	#endif // DEBUG_TRACE_EXECUTION
	#pragma endregion

	while (true) {
		// Handles thread pausing
		if (!fut && memory::gc.shouldCollect.load()) {
			// If fut is null, this is the main thread of execution which runs the GC
			if (!vm->threadsPauseFlag.load()) vm->threadsPauseFlag.store(true);

		}
		else if(vm->threadsPauseFlag.load()){
			paused.store(true);
			while (vm->threadsPauseFlag.load()) {

			}
			paused.store(false);
		}
		#ifdef DEBUG_TRACE_EXECUTION
			std::cout << "          ";
			for (Value* slot = stack; slot < stackTop; slot++) {
				std::cout << "[";
				(*slot).print();
				std::cout << "] ";
			}
			std::cout << "\n";
			disassembleInstruction(&frame->closure->func->body, frames[frameCount - 1].ip);
		#endif
		uint8_t instruction;
		switch (instruction = readByte()) {

		#pragma region Helpers
		case +OpCode::POP: {
			stackTop--;
			break;
		}
		case +OpCode::POPN: {
			uint8_t nToPop = readByte();
			stackTop -= nToPop;
			break;
		}
		case +OpCode::LOAD_INT: {
			push(Value(static_cast<double>(readByte())));
			break;
		}
		#pragma endregion

		#pragma region Constants
		case +OpCode::CONSTANT: {
			Value constant = readConstant();
			push(constant);
			break;
		}
		case +OpCode::CONSTANT_LONG: {
			Value constant = readConstantLong();
			push(constant);
			break;
		}
		case +OpCode::NIL: push(Value::nil()); break;
		case +OpCode::TRUE: push(Value(true)); break;
		case +OpCode::FALSE: push(Value(false)); break;
		#pragma endregion

		#pragma region Unary
		case +OpCode::NEGATE: {
			Value val = pop();
			if (!val.isNumber()) {
				runtimeError(std::format("Operand must be a number, got {}.", val.typeToStr()));
			}
			push(Value(-get<double>(val.value)));
			break;
		}
		case +OpCode::NOT: {
			push(Value(isFalsey(pop())));
			break;
		}
		case +OpCode::BIN_NOT: {
			// Doing implicit conversion from double to long long, could end up with precision errors
			Value val = pop();
			if (!val.isNumber()) {
				runtimeError(std::format("Operand must be a number, got {}.", peek(0).typeToStr()));
			}
			if (!IS_INT(get<double>(val.value))) {
				runtimeError("Number must be a integer, got a float.");
			}
			double num = get<double>(pop().value);
			// Cursed as shit
			long long temp = static_cast<long long>(num);
			temp = ~temp;
			push(Value(static_cast<double>(temp)));
			break;
		}
		case +OpCode::INCREMENT: {
			byte arg = readByte();
			int8_t sign = (arg & 0b00000001) == 1 ? 1 : -1;
			// True: prefix, false: postfix
			bool isPrefix = (arg & 0b00000010) == 1;

			byte type = arg >> 2;

			auto tryIncrement = [&](Value& val) {
				if (!val.isNumber()) {
					runtimeError(std::format("Operand must be a number, got {}.", val.typeToStr()));
				}
				if (isPrefix) {
					val.value = get<double>(val.value) + sign;
					push(val);
				}
				else {
					push(val);
					val.value = get<double>(val.value) + sign;
				}
			};

			switch (type) {
			case 0: {
				byte slot = readByte();
				Value& num = frame->slots[slot];
				// If this is a local upvalue
				if (num.isUpvalue()) {
					Value& temp = num.asUpvalue()->val;
					tryIncrement(temp);
					break;
				}
				tryIncrement(num);
				break;
			}
			case 1: {
				byte slot = readByte();
				Value& num = frame->closure->upvals[slot]->val;
				tryIncrement(num);
				break;
			}
			case 2: {
				byte index = readByte();
				Globalvar& var = vm->globals[index];
				if (!var.isDefined) {
					runtimeError(std::format("Undefined variable '{}'.", var.name));
				}
				tryIncrement(var.val);
				break;
			}
			case 3: {
				byte index = readShort();
				Globalvar& var = vm->globals[index];
				if (!var.isDefined) {
					runtimeError(std::format("Undefined variable '{}'.", var.name));
				}
				tryIncrement(var.val);
				break;
			}
			case 4: {
				Value inst = pop();
				if (!inst.isInstance()) {
					runtimeError(std::format("Only instances/structs have properties, got {}.", inst.typeToStr()));
				}

				object::ObjInstance* instance = inst.asInstance();
				string str = readString()->str;
				auto it = instance->fields.find(str);
				if (it == instance->fields.end()) {
					runtimeError(std::format("Field '{}' doesn't exist.", str));
				}
				Value& num = it->second;
				if (isPrefix) {
					tryIncrement(num);
				}
				else {
					tryIncrement(num);
				}
				break;
			}
			case 5: {
				Value inst = pop();
				if (!inst.isInstance()) {
					runtimeError(std::format("Only instances/structs have properties, got {}.", inst.typeToStr()));
				}

				object::ObjInstance* instance = inst.asInstance();
				string str = readStringLong()->str;

				auto it = instance->fields.find(str);
				if (it == instance->fields.end()) {
					runtimeError(std::format("Field '{}' doesn't exist.", str));
				}
				Value& num = it->second;
				if (isPrefix) {
					tryIncrement(num);
				}
				else {
					tryIncrement(num);
				}
				break;
			}
			case 6: {
				Value field = pop();
				Value callee = pop();
				if (!callee.isArray() && !callee.isInstance()) runtimeError(std::format("Expected a array or struct, got {}.", callee.typeToStr()));
				
				switch (get<object::Obj*>(callee.value)->type) {
				case object::ObjType::ARRAY: {
					object::ObjArray* arr = callee.asArray();
					uInt64 index = checkArrayBounds(field, callee);
					Value& num = arr->values[index];
					tryIncrement(num);
					break;
				}
				case object::ObjType::INSTANCE: {
					if (!field.isString()) runtimeError(std::format("Expected a string for field name, got {}.", field.typeToStr()));

					object::ObjInstance* instance = callee.asInstance();
					string str = field.asString()->str;

					auto it = instance->fields.find(str);
					if (it == instance->fields.end()) {
						runtimeError(std::format("Field '{}' doesn't exist.", str));
					}
					Value& num = it->second;
					if (isPrefix) {
						tryIncrement(num);
					}
					else {
						tryIncrement(num);
					}
					break;
				}
				}
				break;
			}
			}
			break;
		}
		#pragma endregion

		#pragma region Binary
		case +OpCode::BITWISE_XOR: INT_BINARY_OP(NUMBER_VAL, ^); break;
		case +OpCode::BITWISE_OR: INT_BINARY_OP(NUMBER_VAL, | ); break;
		case +OpCode::BITWISE_AND: INT_BINARY_OP(NUMBER_VAL, &); break;
		case +OpCode::ADD: {
			if (peek(0).isString() && peek(1).isString()) {
				object::ObjString* b = pop().asString();
				object::ObjString* a = pop().asString();

				push(Value(a->concat(b)));
			}
			else if (peek(0).isNumber() && peek(1).isNumber()) {
				double b = get<double>(pop().value);
				double a = get<double>(pop().value);
				push(Value(a + b));
			}
			else {
				runtimeError(std::format("Operands must be two numbers or two strings, got {} and {}.",
					peek(1).typeToStr(), peek(0).typeToStr()));
			}
			break;
		}
		case +OpCode::SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
		case +OpCode::MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
		case +OpCode::DIVIDE:   BINARY_OP(NUMBER_VAL, / ); break;
		case +OpCode::MOD:	  INT_BINARY_OP(NUMBER_VAL, %); break;
		case +OpCode::BITSHIFT_LEFT: INT_BINARY_OP(NUMBER_VAL, << ); break;
		case +OpCode::BITSHIFT_RIGHT: INT_BINARY_OP(NUMBER_VAL, >> ); break;
		#pragma endregion

		#pragma region Binary that returns bools
		case +OpCode::EQUAL: {
			Value b = pop();
			Value a = pop();
			push(Value(a == b));
			break;
		}
		case +OpCode::NOT_EQUAL: {
			Value b = pop();
			Value a = pop();
			push(Value(a != b));
			break;
		}
		case +OpCode::GREATER: BINARY_OP(BOOL_VAL, > ); break;
		case +OpCode::GREATER_EQUAL: {
			//Have to do this because of floating point comparisons
			if (!peek(0).isNumber() || !peek(1).isNumber()) {
				runtimeError(std::format("Operands must be two numbers, got {} and {}.", peek(1).typeToStr(), peek(0).typeToStr()));
			}
			double b = get<double>(pop().value);
			double a = get<double>(pop().value);
			if (a > b || FLOAT_EQ(a, b)) push(Value(true));
			else push(Value(false));
			break;
		}
		case +OpCode::LESS: BINARY_OP(BOOL_VAL, < ); break;
		case +OpCode::LESS_EQUAL: {
			//Have to do this because of floating point comparisons
			if (!peek(0).isNumber() || !peek(1).isNumber()) {
				runtimeError(std::format("Operands must be two numbers, got {} and {}.", peek(1).typeToStr(), peek(0).typeToStr()));
			}
			double b = get<double>(pop().value);
			double a = get<double>(pop().value);
			if (a < b || FLOAT_EQ(a, b)) push(Value(true));
			else push(Value(false));
			break;
		}
		#pragma endregion

		#pragma region Statements and vars
		case +OpCode::PRINT: {
			pop().print();
			std::cout << "\n";
			break;
		}

		case +OpCode::DEFINE_GLOBAL: {
			byte index = readByte();
			vm->globals[index].val = pop();
			vm->globals[index].isDefined = true;
			break;
		}
		case +OpCode::DEFINE_GLOBAL_LONG: {
			uInt index = readShort();
			vm->globals[index].val = pop();
			vm->globals[index].isDefined = true;
			break;
		}

		case +OpCode::GET_GLOBAL: {
			byte index = readByte();
			Globalvar& var = vm->globals[index];
			if (!var.isDefined) {
				runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			push(var.val);
			break;
		}
		case +OpCode::GET_GLOBAL_LONG: {
			uInt index = readShort();
			Globalvar& var = vm->globals[index];
			if (!var.isDefined) {
				runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			push(var.val);
			break;
		}

		case +OpCode::SET_GLOBAL: {
			byte index = readByte();
			Globalvar& var = vm->globals[index];
			if (!var.isDefined) {
				runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			var.val = peek(0);
			break;
		}
		case +OpCode::SET_GLOBAL_LONG: {
			uInt index = readShort();
			Globalvar& var = vm->globals[index];
			if (!var.isDefined) {
				runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			var.val = peek(0);
			break;
		}

		case +OpCode::GET_LOCAL: {
			uint8_t slot = readByte();
			Value val = frame->slots[slot];
			if (val.isUpvalue()) {
				push(val.asUpvalue()->val);
				break;
			}
			push(frame->slots[slot]);
			break;
		}

		case +OpCode::SET_LOCAL: {
			uint8_t slot = readByte();
			Value val = frame->slots[slot];
			if (val.isUpvalue()) {
				val.asUpvalue()->val = peek(0);
				break;
			}
			frame->slots[slot] = peek(0);
			break;
		}

		case +OpCode::GET_UPVALUE: {
			uint8_t slot = readByte();
			push(frame->closure->upvals[slot]->val);
			break;
		}
		case +OpCode::SET_UPVALUE: {
			uint8_t slot = readByte();
			frame->closure->upvals[slot]->val = peek(0);
			break;
		}
		#pragma endregion

		#pragma region Control flow
		case +OpCode::JUMP: {
			uint16_t offset = readShort();
			frame->ip += offset;
			break;
		}

		case +OpCode::JUMP_IF_FALSE: {
			uint16_t offset = readShort();
			if (isFalsey(peek(0))) frame->ip += offset;
			break;
		}

		case +OpCode::JUMP_IF_TRUE: {
			uint16_t offset = readShort();
			if (!isFalsey(peek(0))) frame->ip += offset;
			break;
		}

		case +OpCode::JUMP_IF_FALSE_POP: {
			uint16_t offset = readShort();
			if (isFalsey(pop())) frame->ip += offset;
			break;
		}

		case +OpCode::LOOP_IF_TRUE: {
			uint16_t offset = readShort();
			if (!isFalsey(pop())) frame->ip -= offset;
			break;
		}

		case +OpCode::LOOP: {
			uint16_t offset = readShort();
			frame->ip -= offset;
			break;
		}

		case +OpCode::JUMP_POPN: {
			uInt16 offset = readShort();
			stackTop -= readByte();
			frame->ip += offset;
			break;
		}

		case +OpCode::SWITCH: {
			Value val = pop();
			uInt caseNum = readShort();
			uInt offset = frame->ip + caseNum;
			int jumpOffset = -1;
			for (int i = 0; i < caseNum; i++) {
				if (val == readConstant()) {
					jumpOffset = offset + (i * 2);
					break;
				}
			}
			if (jumpOffset == -1) jumpOffset = offset + caseNum * 2;
			frame->ip = jumpOffset;
			uInt jmp = readShort();
			frame->ip += jmp;
			break;
		}
		case +OpCode::SWITCH_LONG: {
			Value val = pop();
			uInt caseNum = readShort();
			uInt offset = frame->ip + caseNum * 2;
			int jumpOffset = -1;
			for (int i = 0; i < caseNum; i++) {
				if (val == readConstantLong()) {
					jumpOffset = offset + (i * 2);
					break;
				}
			}
			if (jumpOffset == -1) jumpOffset = offset + caseNum * 2;
			frame->ip = jumpOffset;
			uInt jmp = readShort();
			frame->ip += jmp;
			break;
		}
		#pragma endregion

		#pragma region Functions
		case +OpCode::CALL: {
			// How many values are on the stack right now
			int argCount = readByte();
			if (!callValue(peek(argCount), argCount)) {
				return;
			}
			// If the call is succesful, there is a new call frame, so we need to update the pointer
			frame = &frames[frameCount - 1];
			break;
		}

		case +OpCode::RETURN: {
			Value result = pop();
			frameCount--;
			//if we're returning from the implicit funcition
			if (frameCount == 0) {
				Value val = pop();
				//if this is a child thread that has a future attached to it, assign the value to the future
				if (fut) fut->val = val;
				return;
			}

			stackTop = frame->slots;
			push(result);
			//if the call is succesful, there is a new call frame, so we need to update the pointer
			frame = &frames[frameCount - 1];
			break;
		}

		case +OpCode::CLOSURE: {
			object::ObjClosure* closure = new object::ObjClosure(readConstant().asFunction());
			for (int i = 0; i < closure->upvals.size(); i++) {
				uint8_t isLocal = readByte();
				uint8_t index = readByte();
				if (isLocal) {
					closure->upvals[i] = captureUpvalue(frame->slots + index);
				}
				else {
					closure->upvals[i] = frame->closure->upvals[index];
				}
			}
			push(Value(closure));
			break;
		}
		case +OpCode::CLOSURE_LONG: {
			object::ObjClosure* closure = new object::ObjClosure(readConstantLong().asFunction());
			for (int i = 0; i < closure->upvals.size(); i++) {
				uint8_t isLocal = readByte();
				uint8_t index = readByte();
				if (isLocal) {
					closure->upvals[i] = captureUpvalue(frame->slots + index);
				}
				else {
					closure->upvals[i] = frame->closure->upvals[index];
				}
			}
			push(Value(closure));
			break;
		}
		#pragma endregion

		#pragma region Multithreading
		case +OpCode::LAUNCH_ASYNC: {
			byte argCount = readByte();
			Thread* t = new Thread(vm);
			t->startThread(&stackTop[-1 - argCount], argCount + 1);
			stackTop -= argCount + 1;
			// Only one thread can add/remove a new child thread at any time
			std::scoped_lock(vm->mtx);
			vm->childThreads.push_back(t);
			push(new object::ObjFuture(t));
		}

		case +OpCode::AWAIT: {
			Value val = pop();
			if (!val.isFuture()) runtimeError(std::format("Await can only be applied to a future, got {}", val.typeToStr()));
			object::ObjFuture* fut = val.asFuture();
			fut->fut.wait();
			std::scoped_lock(vm->mtx);
			for (int i = vm->childThreads.size(); i >= 0; i--) {
				Thread* t = vm->childThreads[i];
				delete t;
				vm->childThreads.erase(vm->childThreads.begin() + i);
			}
			// Can safely access fut->val from this thread since the value is being read and won't be written to again 
			push(fut->val);
		}
		#pragma endregion

		#pragma region Objects, arrays and maps
		case +OpCode::CREATE_ARRAY: {
			uInt64 size = readByte();
			uInt64 i = 0;
			object::ObjArray* arr = new object::ObjArray(size);
			while (i < size) {
				//size-i to because the values on the stack are in reverse order compared to how they're supposed to be in a array
				Value val = pop();
				//if numOfHeapPtr is 0 we don't trace or update the array when garbage collecting
				if (val.isObj()) arr->numOfHeapPtr++;

				arr->values[size - i - 1] = val;
				i++;
			}
			push(Value(arr));
			break;
		}

		case +OpCode::GET: {
			//structs and objects also get their own +OpCode::GET_PROPERTY operator for access using '.'
			//use peek because in case this is a get call to a instance that has a defined "access" method
			//we want to use these 2 values as args and receiver
			Value field = pop();
			Value callee = pop();
			if (!callee.isArray() && !callee.isInstance())
				return runtimeError(std::format("Expected a array or struct, got {}.", callee.typeToStr()));

			switch (get<object::Obj*>(callee.value)->type) {
			case object::ObjType::ARRAY: {
				object::ObjArray* arr = callee.asArray();
				uInt64 index = checkArrayBounds(field, callee);
				push(arr->values[index]);
				break;
			}
			case object::ObjType::INSTANCE: {
				if (!field.isString()) return runtimeError(std::format("Expected a string for field name, got {}.", field.typeToStr()));

				object::ObjInstance* instance = callee.asInstance();
				string name = field.asString()->str;
				auto it = instance->fields.find(name);
				if (it != instance->fields.end()) {
					push(it->second);
					break;
				}

				if (instance->klass && bindMethod(instance->klass, name)) break;
				runtimeError(std::format("Field '{}' doesn't exist.", name));
				break;
			}
			}
			break;
		}

		case +OpCode::SET: {
			//structs and objects also get their own +OpCode::SET_PROPERTY operator for setting using '.'
			Value field = pop();
			Value callee = pop();
			Value val = peek(0);

			if (!callee.isArray() && !callee.isInstance()) 
				runtimeError(std::format("Expected a array or struct, got {}.", callee.typeToStr()));

			switch (get<object::Obj*>(callee.value)->type) {
			case object::ObjType::ARRAY: {
				object::ObjArray* arr = callee.asArray();
				uInt64 index = checkArrayBounds(field, callee);

				//if numOfHeapPtr is 0 we don't trace or update the array when garbage collecting
				if (val.isObj() && !arr->values[index].isObj()) arr->numOfHeapPtr++;
				else if (!val.isObj() && arr->values[index].isObj()) arr->numOfHeapPtr--;
				arr->values[index] = val;
				break;
			}
			case object::ObjType::INSTANCE: {
				if (!field.isString()) return runtimeError(std::format("Expected a string for field name, got {}.", field.typeToStr()));

				object::ObjInstance* instance = callee.asInstance();
				string str = field.asString()->str;
				//settting will always succeed, and we don't care if we're overriding an existing field, or creating a new one
				instance->fields.insert_or_assign(str, val);
				break;
			}
			}
			break;
		}

		case +OpCode::CLASS: {
			push(Value(new object::ObjClass(readStringLong()->str)));
			break;
		}

		case +OpCode::GET_PROPERTY: {
			Value inst = pop();
			if (!inst.isInstance()) {
				return runtimeError(std::format("Only instances/structs have properties, got {}.", inst.typeToStr()));
			}

			object::ObjInstance* instance = inst.asInstance();
			string name = readString()->str;

			auto it = instance->fields.find(name);
			if (it != instance->fields.end()) {
				push(it->second);
				break;
			}

			if (instance->klass && bindMethod(instance->klass, name)) break;
			runtimeError(std::format("Field '{}' doesn't exist.", name));
			break;
		}
		case +OpCode::GET_PROPERTY_LONG: {
			Value inst = pop();
			if (!inst.isInstance()) {
				return runtimeError(std::format("Only instances/structs have properties, got {}.", inst.typeToStr()));
			}

			object::ObjInstance* instance = inst.asInstance();
			string name = readStringLong()->str;

			auto it = instance->fields.find(name);
			if (it != instance->fields.end()) {
				push(it->second);
				break;
			}

			if (instance->klass && bindMethod(instance->klass, name)) break;
			runtimeError(std::format("Field '{}' doesn't exist.", name));
			break;
		}

		case +OpCode::SET_PROPERTY: {
			Value inst = pop();
			if (!inst.isInstance()) {
				return runtimeError(std::format("Only instances/structs have properties, got {}.", inst.typeToStr()));
			}
			object::ObjInstance* instance = inst.asInstance();

			//we don't care if we're overriding or creating a new field
			instance->fields.insert_or_assign(readString()->str, peek(0));
			break;
		}
		case +OpCode::SET_PROPERTY_LONG: {
			Value inst = pop();
			if (!inst.isInstance()) {
				return runtimeError(std::format("Only instances/structs have properties, got {}.", inst.typeToStr()));
			}
			object::ObjInstance* instance = inst.asInstance();

			//we don't care if we're overriding or creating a new field
			instance->fields.insert_or_assign(readStringLong()->str, peek(0));
			break;
		}

		case +OpCode::CREATE_STRUCT: {
			int numOfFields = readByte();

			//passing null instead of class signals to the VM that this is a struct, and not a instance of a class
			object::ObjInstance* inst = new object::ObjInstance(nullptr);

			//the compiler emits the fields in reverse order, so we can loop through them normally and pop the values on the stack
			for (int i = 0; i < numOfFields; i++) {
				object::ObjString* name = readString();
				inst->fields.insert_or_assign(name->str, pop());
			}
			push(Value(inst));
			break;
		}
		case +OpCode::CREATE_STRUCT_LONG: {
			int numOfFields = readByte();

			//passing null instead of class signals to the VM that this is a struct, and not a instance of a class
			object::ObjInstance* inst = new object::ObjInstance(nullptr);

			//the compiler emits the fields in reverse order, so we can loop through them normally and pop the values on the stack
			for (int i = 0; i < numOfFields; i++) {
				object::ObjString* name = readStringLong();
				inst->fields.insert_or_assign(name->str, pop());
			}
			push(Value(inst));
			break;
		}

		case +OpCode::METHOD: {
			//class that this method binds too
			defineMethod(readStringLong()->str);
			break;
		}

		case +OpCode::INVOKE: {
			//gets the method and calls it immediatelly, without converting it to a objBoundMethod
			string method = readString()->str;
			int argCount = readByte();
			if (!invoke(method, argCount)) {
				return;
			}
			frame = &frames[frameCount - 1];
			break;
		}
		case +OpCode::INVOKE_LONG: {
			//gets the method and calls it immediatelly, without converting it to a objBoundMethod
			string method = readStringLong()->str;
			int argCount = readByte();
			if (!invoke(method, argCount)) {
				return;
			}
			frame = &frames[frameCount - 1];
			break;
		}

		case +OpCode::INHERIT: {
			Value superclass = peek(1);
			if (!superclass.isClass()) {
				return runtimeError(std::format("Superclass must be a class, got {}.", superclass.typeToStr()));
			}
			object::ObjClass* subclass = peek(0).asClass();
			//copy down inheritance
			for (auto it : superclass.asClass()->methods) {
				subclass->methods.insert_or_assign(it.first, it.second);
			}
			break;
		}

		case +OpCode::GET_SUPER: {
			//super is ALWAYS followed by a field
			string name = readString()->str;
			object::ObjClass* superclass = pop().asClass();

			if (!bindMethod(superclass, name)) return;
			break;
		}
		case +OpCode::GET_SUPER_LONG: {
			//super is ALWAYS followed by a field
			string name = readStringLong()->str;
			object::ObjClass* superclass = pop().asClass();

			if (!bindMethod(superclass, name)) return;
			break;
		}

		case +OpCode::SUPER_INVOKE: {
			//works same as +OpCode::INVOKE, but uses invokeFromClass() to specify the superclass
			string method = readString()->str;
			int argCount = readByte();
			object::ObjClass* superclass = pop().asClass();

			if (!invokeFromClass(superclass, method, argCount)) return;
			frame = &frames[frameCount - 1];
			break;
		}
		case +OpCode::SUPER_INVOKE_LONG: {
			//works same as +OpCode::INVOKE, but uses invokeFromClass() to specify the superclass
			string method = readStringLong()->str;
			int argCount = readByte();
			object::ObjClass* superclass = pop().asClass();

			if (!invokeFromClass(superclass, method, argCount)) return;
			frame = &frames[frameCount - 1];
			break;
		}
		#pragma endregion

		}
	}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef INT_BINARY_OP
#undef READ_STRING
#undef READ_SHORT
}
