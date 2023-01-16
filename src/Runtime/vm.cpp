#include "vm.h"
#include "../Codegen/compiler.h"

runtime::VM::VM(compileCore::Compiler* compiler) {
	globals = compiler->globals;
	sourceFiles = compiler->sourceFiles;
	push(Value(compiler->endFuncDecl()));
}


#pragma region Helpers
string runtime::expectedType(string msg, Value val) {
	return msg + val.typeToStr() + ".";
}

byte runtime::VM::getOp(long _ip) {
	return frames[frameCount - 1].closure->func->body.code[_ip];
}

void runtime::VM::push(Value val) {
	if (stackTop >= stack + STACK_MAX) {
		runtimeError("Stack overflow");
		exit(64);
	}
	*stackTop = val;
	stackTop++;
}

Value runtime::VM::pop() {
	stackTop--;
	return *stackTop;
}

void runtime::VM::resetStack() {
	stackTop = stack;
}

Value runtime::VM::peek(int depth) {
	return stackTop[-1 - depth];
}

RuntimeResult runtime::VM::runtimeError(string err) {
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
		string temp = yellow + line.getFileName(sourceFiles) + black + ":" + cyan + std::to_string(line.line) + " | " + black;
		std::cout << temp << "in ";
		std::cout << (function->name == nullptr ? "script" : function->name->getString()) << "()\n";
	}
	resetStack();
	return RuntimeResult::RUNTIME_ERROR;
}

static bool isFalsey(Value value) {
	return ((value.isBool() && !value.asBool()) || value.isNil());
}

bool runtime::VM::callValue(Value callee, int argCount) {
	if (callee.isObj()) {
		switch (callee.asObj()->type) {
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
			Value initializer;
			if (klass->methods.get(klass->name, &initializer)) {
				return call(initializer.asClosure(), argCount);
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

bool runtime::VM::call(object::ObjClosure* closure, int argCount) {
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

object::ObjUpval* runtime::VM::captureUpvalue(Value* local) {
	//first checks to see if there already exists a upvalue that points to stack location 'local'
	for (int i = openUpvals.size() - 1; i >= 0; i--) {
		if (openUpvals[i]->location >= local) {
			if (openUpvals[i]->location == local) return openUpvals[i];
		}
		else break;
	}
	//if not, create a new upvalue
	object::ObjUpval* createdUpval = new object::ObjUpval(local);
	openUpvals.push(createdUpval);
	return createdUpval;
}

void runtime::VM::closeUpvalues(Value* last) {
	object::ObjUpval* upval;
	//close every value on the stack until last
	for (int i = openUpvals.size() - 1; i >= 0; i--) {
		upval = openUpvals[i];
		if (upval->location >= last) {
			upval->closed = *upval->location;
			upval->location = &upval->closed;
			upval->isOpen = false;//this check is used by the GC
		}
		else break;
	}
}

void runtime::VM::defineMethod(object::ObjString* name) {
	//no need to typecheck since the compiler made sure to emit code in this order
	Value method = peek(0);
	object::ObjClass* klass = peek(1).asClass();
	klass->methods.set(name, method);
	//we only pop the method, since other methods we're compiling will also need to know their class
	pop();
}

bool runtime::VM::bindMethod(object::ObjClass* klass, object::ObjString* name) {
	//At the start the instance whose method we're binding needs to be on top of the stack
	Value method;
	if (!klass->methods.get(name, &method)) {
		runtimeError(std::format("{} doesn't contain method '{}'.", klass->name->getString(), name->getString()));
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

bool runtime::VM::invoke(object::ObjString* fieldName, int argCount) {
	Value receiver = peek(argCount);
	if (!receiver.isInstance()) {
		runtimeError(std::format("Only instances can call methods, got {}.", receiver.typeToStr()));
		return false;
	}

	object::ObjInstance* instance = receiver.asInstance();
	Value value;
	//this is here because invoke can also be used on functions stored in a instances field
	if (instance->fields.get(fieldName, &value)) {
		stackTop[-argCount - 1] = value;
		return callValue(value, argCount);
	}
	//this check is used because we also use objInstance to represent struct literals
	//and if this instance is a struct it can only contain functions inside it's field table
	if (instance->klass == nullptr) {
		runtimeError(std::format( "Undefined property '{}'.", fieldName->getString()));
		return false;
	}

	return invokeFromClass(instance->klass, fieldName, argCount);
}

bool runtime::VM::invokeFromClass(object::ObjClass* klass, object::ObjString* name, int argCount) {
	Value method;
	if (!klass->methods.get(name, &method)) {
		runtimeError(std::format("Class '{}' doesn't contain '{}'.", klass->name->getString(), name->getString()));
		return false;
	}
	//the bottom of the call stack will contain the receiver instance
	return call(method.asClosure(), argCount);
}
#pragma endregion

RuntimeResult runtime::VM::execute() {
	CallFrame* frame = &frames[frameCount - 1];
#pragma region Macros
#define READ_BYTE() (getOp(frame->ip++))
#define READ_SHORT() (frame->ip += 2, (uint16_t)((getOp(frame->ip-2) << 8) | getOp(frame->ip-1)))
#define READ_CONSTANT() (frames[frameCount - 1].closure->func->body.constants[READ_BYTE()])
#define READ_CONSTANT_LONG() (frames[frameCount - 1].closure->func->body.constants[READ_SHORT()])
#define READ_STRING() READ_CONSTANT().asString()
#define READ_STRING_LONG() READ_CONSTANT_LONG().asString()
#define BINARY_OP(valueType, op) \
		do { \
			if (!peek(0).isNumber() || !peek(1).isNumber()) { \
			return runtimeError(std::format("Operands must be numbers, got '{}' and '{}'.", peek(1).typeToStr(), peek(0).typeToStr())); \
			} \
			double b = pop().asNum(); \
			double a = pop().asNum(); \
			push(Value(a op b)); \
		} while (false)

#define INT_BINARY_OP(valueType, op)\
		do {\
			if (!peek(0).isNumber() || !peek(1).isNumber()) { \
			return runtimeError(std::format("Operands must be numbers, got '{}' and '{}'.", peek(1).typeToStr(), peek(0).typeToStr())); \
			} \
			double b = pop().asNum(); \
			double a = pop().asNum(); \
			push(Value((double)((uInt64)a op (uInt64)b))); \
		} while (false)

#ifdef DEBUG_TRACE_EXECUTION
	std::cout << "-------------Code execution starts-------------\n";
#endif // DEBUG_TRACE_EXECUTION
#pragma endregion

	while (true) {
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
		switch (instruction = READ_BYTE()) {

		#pragma region Helpers
		case +OpCode::POP:
			stackTop--;
			break;
		case +OpCode::POPN: {
			uint8_t nToPop = READ_BYTE();
			stackTop -= nToPop;
			break;
		}
		#pragma endregion

		#pragma region Constants
		case +OpCode::CONSTANT: {
			Value constant = READ_CONSTANT();
			push(constant);
			break;
		}
		case +OpCode::CONSTANT_LONG: {
			Value constant = READ_CONSTANT_LONG();
			push(constant);
			break;
		}
		case +OpCode::NIL: push(Value::nil()); break;
		case +OpCode::TRUE: push(Value(true)); break;
		case +OpCode::FALSE: push(Value(false)); break;
		#pragma endregion

		#pragma region Unary
		case +OpCode::NEGATE:
			Value val = pop();
			if (!val.isNumber()) {
				return runtimeError(std::format("Operand must be a number, got {}.", val.typeToStr()));
			}
			push(Value(-val.asNum()));
			break;
		case +OpCode::NOT:
			push(Value(isFalsey(pop())));
			break;
		case +OpCode::BIN_NOT: {
			//doing implicit conversion from double to long long, could end up with precision errors
			Value val = pop();
			if (!val.isNumber()) {
				return runtimeError(std::format("Operand must be a number, got {}.", peek(0).typeToStr()));
			}
			if (!IS_INT(val.asNum())) {
				return runtimeError("Number must be a integer, got a float.");
			}
			double num = pop().asNum();
			long long temp = static_cast<long long>(num);
			temp = ~temp;
			push(Value(static_cast<double>(temp)));
			break;
		}
		#pragma endregion

#pragma region Binary
		case +OpCode::ADD: {
			if (peek(0).isString() && peek(1).isString()) {
				concatenate();
			}
			else if (peek(0).isNumber() && peek(1).isNumber()) {
				double b = pop().asNum();
				double a = pop().asNum();
				push(Value(a + b));
			}
			else {
				return runtimeError(std::format("Operands must be two numbers or two strings, got {} and {}.",
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
		case +OpCode::BITWISE_AND: INT_BINARY_OP(NUMBER_VAL, &); break;
		case +OpCode::BITWISE_OR: INT_BINARY_OP(NUMBER_VAL, | ); break;
		case +OpCode::BITWISE_XOR: INT_BINARY_OP(NUMBER_VAL, ^); break;
#pragma endregion

#pragma region Binary that returns bools
		case +OpCode::EQUAL: {
			Value b = pop();
			Value a = pop();
			push(Value(a.equals(b)));
			break;
		}
		case +OpCode::NOT_EQUAL: {
			Value b = pop();
			Value a = pop();
			push(Value(!a.equals(b)));
			break;
		}
		case +OpCode::GREATER: BINARY_OP(BOOL_VAL, > ); break;
		case +OpCode::LESS: BINARY_OP(BOOL_VAL, < ); break; 
		case +OpCode::GREATER_EQUAL: {
			//Have to do this because of floating point comparisons
			if (!peek(0).isNumber() || !peek(1).isNumber()) {
				return runtimeError(std::format("Operands must be two numbers, got {} and {}.",
					peek(1).typeToStr(), peek(0).typeToStr()));
			}
			double b = pop().asNum();
			double a = pop().asNum();
			if (a > b || FLOAT_EQ(a, b)) push(Value(true));
			else push(Value(false));
			break;
		}
		case +OpCode::LESS_EQUAL: {
			//Have to do this because of floating point comparisons
			if (!peek(0).isNumber() || !peek(1).isNumber()) {
				return runtimeError(std::format("Operands must be two numbers, got {} and {}.",
					peek(1).typeToStr(), peek(0).typeToStr()));
			}
			double b = pop().asNum();
			double a = pop().asNum();
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
			byte index = READ_BYTE();
			globals[index].val = pop();
			globals[index].isDefined = true;
			break;
		}
		case +OpCode::DEFINE_GLOBAL_LONG: {
			uInt index = READ_SHORT();
			globals[index].val = pop();
			globals[index].isDefined = true;
			break;
		}

		case +OpCode::GET_GLOBAL: {
			byte index = READ_BYTE();
			Globalvar& var = globals[index];
			Value value;
			if (!var.isDefined) {
				return runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			push(var.val);
			break;
		}
		case +OpCode::GET_GLOBAL_LONG: {
			uInt index = READ_SHORT();
			Globalvar& var = globals[index];
			Value value;
			if (!var.isDefined) {
				return runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			push(var.val);
			break;
		}

		case +OpCode::SET_GLOBAL: {
			byte index = READ_BYTE();
			Globalvar& var = globals[index];
			if (!var.isDefined) {
				return runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			var.val = peek(0);
			break;
		}
		case +OpCode::SET_GLOBAL_LONG: {
			uInt index = READ_SHORT();
			Globalvar& var = globals[index];
			if (!var.isDefined) {
				return runtimeError(std::format("Undefined variable '{}'.", var.name));
			}
			var.val = peek(0);
			break;
		}

		case +OpCode::GET_LOCAL: {
			uint8_t slot = READ_BYTE();
			push(frame->slots[slot]);
			break;
		}

		case +OpCode::SET_LOCAL: {
			uint8_t slot = READ_BYTE();
			frame->slots[slot] = peek(0);
			break;
		}
						 //upvals[slot]->location can be a pointer to either a stack slot, or a closed upvalue in a functions closure
		case +OpCode::GET_UPVALUE: {
			uint8_t slot = READ_BYTE();
			push(*frame->closure->upvals[slot]->location);
			break;
		}

		case +OpCode::SET_UPVALUE: {
			uint8_t slot = READ_BYTE();
			*frame->closure->upvals[slot]->location = peek(0);
			break;
		}

		case +OpCode::CLOSE_UPVALUE: {
			closeUpvalues(stackTop - 1);
			pop();
			break;
		}
#pragma endregion

#pragma region Control flow
		case +OpCode::JUMP_IF_TRUE: {
			uint16_t offset = READ_SHORT();
			if (!isFalsey(peek(0))) frame->ip += offset;
			break;
		}

		case +OpCode::JUMP_IF_FALSE: {
			uint16_t offset = READ_SHORT();
			if (isFalsey(peek(0))) frame->ip += offset;
			break;
		}

		case +OpCode::JUMP_IF_FALSE_POP: {
			uint16_t offset = READ_SHORT();
			if (isFalsey(pop())) frame->ip += offset;
			break;
		}

		case +OpCode::JUMP: {
			uint16_t offset = READ_SHORT();
			frame->ip += offset;
			break;
		}

		case +OpCode::LOOP: {
			uint16_t offset = READ_SHORT();
			frame->ip -= offset;
			break;
		}

		case +OpCode::JUMP_POPN: {
			uint16_t toPop = READ_SHORT();
			stackTop -= toPop;
			uint16_t offset = READ_SHORT();
			frame->ip += offset;
			break;
		}

		case +OpCode::SWITCH: {
			if (IS_STRING(peek(0)) || IS_NUMBER(peek(0))) {
				int pos = READ_BYTE();
				switchTable& _table = frames[frameCount - 1].closure->func->body.switchTables[pos];
				//default jump exists for every switch, and if it's not user defined it jumps to the end of switch
				switch (_table.type) {
				case switchType::NUM: {
					//if it's a all number switch stmt, and we have something that isn't a number, we immediatelly jump to default
					if (IS_NUMBER(peek(0))) {
						int num = AS_NUMBER(pop());
						long jumpLength = _table.getJump(num);
						if (jumpLength != -1) {
							frame->ip += jumpLength;
							break;
						}
					}
					frame->ip += _table.defaultJump;
					break;
				}
				case switchType::STRING: {
					if (IS_STRING(peek(0))) {
						objString* str = AS_STRING(pop());
						string key;
						key.assign(str->str, str->length + 1);
						long jumpLength = _table.getJump(key);
						if (jumpLength != -1) {
							frame->ip += jumpLength;
							break;
						}
					}
					frame->ip += _table.defaultJump;
					break;
				}
				case switchType::MIXED: {
					string str;
					//this can be either a number or a string, so we need more checks
					if (IS_STRING(peek(0))) {
						objString* key = AS_STRING(pop());
						str.assign(key->str, key->length + 1);
					}
					else str = std::to_string((int)AS_NUMBER(pop()));
					long jumpLength = _table.getJump(str);
					if (jumpLength != -1) {
						frame->ip += jumpLength;
						break;
					}
					frame->ip += _table.defaultJump;
					break;
				}
				}
			}
			else {
				return runtimeError("Switch expression can be only string or number, got %s.", valueTypeToStr(peek(0)).c_str());
			}
			break;
		}
#pragma endregion

#pragma region Functions
		case +OpCode::CALL: {
			//how many values are on the stack right now
			int argCount = READ_BYTE();
			if (!callValue(peek(argCount), argCount)) {
				return RuntimeResult::RUNTIME_ERROR;
			}
			//if the call is succesful, there is a new call frame, so we need to update the pointer
			frame = &frames[frameCount - 1];
			break;
		}

		case +OpCode::RETURN: {
			Value result = pop();
			closeUpvalues(frame->slots);
			frameCount--;
			//if we're returning from the implicit funcition
			if (frameCount == 0) {
				Value val = pop();
				return RuntimeResult::OK;
			}

			stackTop = frame->slots;
			push(result);
			//if the call is succesful, there is a new call frame, so we need to update the pointer
			frame = &frames[frameCount - 1];
			break;
		}

		case +OpCode::CLOSURE: {
			object::ObjClosure* closure = new object::ObjClosure(READ_CONSTANT().asFunction());
			for (int i = 0; i < closure->upvals.size(); i++) {
				uint8_t isLocal = READ_BYTE();
				uint8_t index = READ_BYTE();
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
			object::ObjClosure* closure = new object::ObjClosure(READ_CONSTANT_LONG().asFunction());
			for (int i = 0; i < closure->upvals.size(); i++) {
				uint8_t isLocal = READ_BYTE();
				uint8_t index = READ_BYTE();
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

#pragma region Objects, arrays and maps
		case +OpCode::CREATE_ARRAY: {
			uInt64 size = READ_BYTE();
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

			switch (callee.asObj()->type) {
			case object::ObjType::ARRAY: {
				if (!field.isNumber()) return runtimeError(std::format("Index must be a number, got {}.", callee.typeToStr()));
				double index = field.asNum();
				object::ObjArray* arr = callee.asArray();
				//Trying to access a variable using a float is a error
				if (IS_INT(index)) return runtimeError("Expected interger, got float.");
				if (index < 0 || index > arr->values.size() - 1)
					return runtimeError(std::format("Index {} outside of range [0, {}].", (uInt64)index, callee.asArray()->values.size() - 1));

				push(arr->values[(uInt64)index]);
				break;
			}
			case object::ObjType::INSTANCE: {
				if (!field.isString()) return runtimeError(std::format("Expected a string for field name, got {}.", field.typeToStr()));

				object::ObjInstance* instance = callee.asInstance();
				object::ObjString* name = field.asString();
				Value value;

				if (instance->fields.get(name, &value)) {
					push(value);
					break;
				}

				if (instance->klass && bindMethod(instance->klass, name)) break;
				//if the field doesn't exist, we push nil as a sentinel value, since this is not considered a runtime error
				push(Value::nil());
				break;
			}
			}
			break;
		}

		case +OpCode::SET: {
			//structs and objects also get their own +OpCode::SET_PROPERTY operator for setting using '.'
			Value val = peek(0);
			Value field = peek(1);
			Value callee = peek(2);
			//if we encounter a user defined set expr, we will want to exit early and not pop all of the above values as they will be used as args
			bool earlyExit = false;

			if (!callee.isArray() && !callee.isInstance())
				return runtimeError(std::format("Expected a array or struct, got {}.", callee.typeToStr()));

			switch (callee.asObj()->type) {
			case object::ObjType::ARRAY: {
				object::ObjArray* arr = callee.asArray();

				if (!field.isNumber()) return runtimeError(std::format("Index has to be a number, got {}.", field.typeToStr()));
				double index = field.asNum();
				//accessing array with a float is a error
				if (IS_INT(index)) return runtimeError("Index has to be a integer.");
				if (index < 0 || index > arr->values.size() - 1)
					return runtimeError(std::format("Index {} outside of range [0, {}].", (uInt64)index, arr->values.size() - 1));

				//if numOfHeapPtr is 0 we don't trace or update the array when garbage collecting
				if (val.isObj() && !arr->values[index].isObj()) arr->numOfHeapPtr++;
				else if (!val.isObj() && arr->values[index].isObj()) arr->numOfHeapPtr--;
				arr->values[(uInt64)index] = val;
				break;
			}
			case object::ObjType::INSTANCE: {
				if (!field.isString()) return runtimeError(std::format("Expected a string for field name, got {}.", field.typeToStr()));

				object::ObjInstance* instance = callee.asInstance();
				object::ObjString* str = field.asString();
				//settting will always succeed, and we don't care if we're overriding an existing field, or creating a new one
				instance->fields.set(str, val);
				break;
			}
			}
			if (earlyExit) break;
			//we want only the value to remain on the stack, since set is a assignment expr
			pop();
			pop();
			pop();
			push(val);
			break;
		}

		case +OpCode::CLASS: {
			push(Value(new object::ObjClass(READ_STRING_LONG())));
			break;
		}

		case +OpCode::GET_PROPERTY: {
			if (!IS_INSTANCE(peek(0))) {
				return runtimeError("Only instances/structs have properties, got %s.", valueTypeToStr(peek(0)).c_str());
			}

			objInstance* instance = AS_INSTANCE(peek(0));
			objString* name = READ_STRING();

			Value value;
			if (instance->table.get(name, &value)) {
				pop(); // Instance.
				push(value);
				break;
			}
			//first check is because structs(instances with no class) are also represented using objInstance
			if (instance->klass && bindMethod(instance->klass, name)) break;
			//if 'name' isn't a field property, nor is it a method, we push nil as a sentinel value
			push(NIL_VAL());
			break;
		}
		case +OpCode::GET_PROPERTY_LONG: {
			if (!IS_INSTANCE(peek(0))) {
				return runtimeError("Only instances/structs have properties, got %s.", valueTypeToStr(peek(0)).c_str());
			}

			objInstance* instance = AS_INSTANCE(peek(0));
			objString* name = READ_STRING_LONG();

			Value value;
			if (instance->table.get(name, &value)) {
				pop(); // Instance.
				push(value);
				break;
			}
			//first check is because structs(instances with no class) are also represented using objInstance
			if (instance->klass && bindMethod(instance->klass, name)) break;
			//if 'name' isn't a field property, nor is it a method, we push nil as a sentinel value
			push(NIL_VAL());
			break;
		}

		case +OpCode::SET_PROPERTY: {
			if (!peek(1).isInstance()) {
				return runtimeError(std::format("Only instances/structs have properties, got {}.", peek(1).typeToStr()));
			}

			object::ObjInstance* instance = peek(1).asInstance();
			//we don't care if we're overriding or creating a new field
			instance->fields.set(READ_STRING(), peek(0));

			Value value = pop();
			pop();
			push(value);
			break;
		}
		case +OpCode::SET_PROPERTY_LONG: {
			if (!peek(1).isInstance()) {
				return runtimeError(std::format("Only instances/structs have properties, got {}.", peek(1).typeToStr()));
			}

			object::ObjInstance* instance = peek(1).asInstance();
			//we don't care if we're overriding or creating a new field
			instance->fields.set(READ_STRING_LONG(), peek(0));

			Value value = pop();
			pop();
			push(value);
			break;
		}

		case +OpCode::CREATE_STRUCT: {
			int numOfFields = READ_BYTE();

			//passing null instead of class signals to the VM that this is a struct, and not a instance of a class
			object::ObjInstance* inst = new object::ObjInstance(nullptr);

			//the compiler emits the fields in reverse order, so we can loop through them normally and pop the values on the stack
			for (int i = 0; i < numOfFields; i++) {
				object::ObjString* name = READ_STRING();
				inst->fields.set(name, pop());
			}
			push(Value(inst));
			break;
		}
		case +OpCode::CREATE_STRUCT_LONG: {
			int numOfFields = READ_BYTE();

			//passing null instead of class signals to the VM that this is a struct, and not a instance of a class
			object::ObjInstance* inst = new object::ObjInstance(nullptr);

			//the compiler emits the fields in reverse order, so we can loop through them normally and pop the values on the stack
			for (int i = 0; i < numOfFields; i++) {
				object::ObjString* name = READ_STRING_LONG();
				inst->fields.set(name, pop());
			}
			push(Value(inst));
			break;
		}

		case +OpCode::METHOD: {
			//class that this method binds too
			defineMethod(READ_STRING_LONG());
			break;
		}

		case +OpCode::INVOKE: {
			//gets the method and calls it immediatelly, without converting it to a objBoundMethod
			object::ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			if (!invoke(method, argCount)) {
				return RuntimeResult::RUNTIME_ERROR;
			}
			frame = &frames[frameCount - 1];
			break;
		}
		case +OpCode::INVOKE_LONG: {
			//gets the method and calls it immediatelly, without converting it to a objBoundMethod
			object::ObjString* method = READ_STRING_LONG();
			int argCount = READ_BYTE();
			if (!invoke(method, argCount)) {
				return RuntimeResult::RUNTIME_ERROR;
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
			subclass->methods.tableAddAll(&subclass->methods);
			break;
		}

		case +OpCode::GET_SUPER: {
			//super is ALWAYS followed by a field
			object::ObjString* name = READ_STRING();
			object::ObjClass* superclass = pop().asClass();

			if (!bindMethod(superclass, name)) {
				return RuntimeResult::RUNTIME_ERROR;
			}
			break;
		}
		case +OpCode::GET_SUPER_LONG: {
			//super is ALWAYS followed by a field
			object::ObjString* name = READ_STRING_LONG();
			object::ObjClass* superclass = pop().asClass();

			if (!bindMethod(superclass, name)) {
				return RuntimeResult::RUNTIME_ERROR;
			}
			break;
		}

		case +OpCode::SUPER_INVOKE: {
			//works same as +OpCode::INVOKE, but uses invokeFromClass() to specify the superclass
			object::ObjString* method = READ_STRING();
			int argCount = READ_BYTE();
			object::ObjClass* superclass = pop().asClass();

			if (!invokeFromClass(superclass, method, argCount)) {
				return RuntimeResult::RUNTIME_ERROR;
			}
			frame = &frames[frameCount - 1];
			break;
		}
		case +OpCode::SUPER_INVOKE_LONG: {
			//works same as +OpCode::INVOKE, but uses invokeFromClass() to specify the superclass
			object::ObjString* method = READ_STRING_LONG();
			int argCount = READ_BYTE();
			object::ObjClass* superclass = pop().asClass();

			if (!invokeFromClass(superclass, method, argCount)) {
				return RuntimeResult::RUNTIME_ERROR;
			}
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

	//this is hit *only* when we pause the fiber, when a fiber finishes executing all it's code it hits the implicit return which handles that
	return RuntimeResult::RUNTIME_ERROR;
}