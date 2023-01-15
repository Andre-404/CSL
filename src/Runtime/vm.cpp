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
				runtimeError("Expected %d arguments for function call but got %d.", arity, argCount);
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
				const char* name = globals.getKey(callee)->str;//gets the name of the native func
				runtimeError("Error in %s: %s", name, str.c_str());
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
				runtimeError("Class constructor expects 0 arguments but got %d.", argCount);
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
		runtimeError("Expected %d arguments for function call but got %d.", closure->func->arity, argCount);
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
		runtimeError("%s doesn't contain method '%s'.", klass->name->str, name->str);
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
		runtimeError("Only instances can call methods, got %s.", valueTypeToStr(receiver).c_str());
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
		runtimeError("Undefined property '%s'.", fieldName->str);
		return false;
	}

	return invokeFromClass(instance->klass, fieldName, argCount);
}

bool runtime::VM::invokeFromClass(object::ObjClass* klass, object::ObjString* name, int argCount) {
	Value method;
	if (!klass->methods.get(name, &method)) {
		runtimeError("Class '%s' doesn't contain '%s'.", klass->name->str, name->str);
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
			if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			return runtimeError("Operands must be numbers, got '%s' and '%s'.", valueTypeToStr(peek(1)).c_str(), valueTypeToStr(peek(0)).c_str()); \
			} \
			double b = AS_NUMBER(pop()); \
			double a = AS_NUMBER(pop()); \
			push(valueType(a op b)); \
		} while (false)

#define INT_BINARY_OP(valueType, op)\
		do {\
			if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			return runtimeError("Operands must be numbers, got '%s' and '%s'.", valueTypeToStr(peek(1)).c_str(), valueTypeToStr(peek(0)).c_str()); \
			} \
			double b = AS_NUMBER(pop()); \
			double a = AS_NUMBER(pop()); \
			push(valueType((double)((uInt64)a op (uInt64)b))); \
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
			printValue(*slot);
			std::cout << "] ";
		}
		std::cout << "\n";
		disassembleInstruction(&frame->closure->func->body, frames[frameCount - 1].ip);
#endif

		uint8_t instruction;
		switch (instruction = READ_BYTE()) {

#pragma region Helpers
		case +OpCode::POP:
			pop();
			break;
		case +OpCode::POPN: {
			uint8_t nToPop = READ_BYTE();
			stackTop -= nToPop;
			break;
		}
#pragma endregion

#pragma region Constants
		case OP_CONSTANT: {
			Value constant = READ_CONSTANT();
			push(constant);
			break;
		}
		case OP_CONSTANT_LONG: {
			Value constant = READ_CONSTANT_LONG();
			push(constant);
			break;
		}
		case OP_NIL: push(NIL_VAL()); break;
		case OP_TRUE: push(BOOL_VAL(true)); break;
		case OP_FALSE: push(BOOL_VAL(false)); break;
#pragma endregion

#pragma region Unary
		case OP_NEGATE:
			if (!IS_NUMBER(peek(0))) {
				return runtimeError("Operand must be a number, got %s.", valueTypeToStr(peek(0)).c_str());
			}
			push(NUMBER_VAL(-AS_NUMBER(pop())));
			break;
		case OP_NOT:
			push(BOOL_VAL(isFalsey(pop())));
			break;
		case OP_BIN_NOT: {
			if (!IS_NUMBER(peek(0))) {
				return runtimeError("Operand must be a number, got %s.", valueTypeToStr(peek(0)).c_str());
			}
			int num = AS_NUMBER(pop());
			num = ~num;
			push(NUMBER_VAL((double)num));
			break;
		}
#pragma endregion

#pragma region Binary
		case OP_ADD: {
			if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
				concatenate();
			}
			else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = AS_NUMBER(pop());
				double a = AS_NUMBER(pop());
				push(NUMBER_VAL(a + b));
			}
			else {
				return runtimeError("Operands must be two numbers or two strings, got %s and %s.",
					valueTypeToStr(peek(1)).c_str(), valueTypeToStr(peek(0)));
			}
			break;
		}
		case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
		case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
		case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, / ); break;
		case OP_MOD:	  INT_BINARY_OP(NUMBER_VAL, %); break;
		case OP_BITSHIFT_LEFT: INT_BINARY_OP(NUMBER_VAL, << ); break;
		case OP_BITSHIFT_RIGHT: INT_BINARY_OP(NUMBER_VAL, >> ); break;
		case OP_BITWISE_AND: INT_BINARY_OP(NUMBER_VAL, &); break;
		case OP_BITWISE_OR: INT_BINARY_OP(NUMBER_VAL, | ); break;
		case OP_BITWISE_XOR: INT_BINARY_OP(NUMBER_VAL, ^); break;
		case OP_ADD_1: {
			if (IS_NUMBER(peek(0))) {
				Value num = pop();
				num.as.num++;
				push(num);
			}
			else return runtimeError("Operands must be two numbers, got %s and number.", valueTypeToStr(peek(0)).c_str());
			break;
		}
		case OP_SUBTRACT_1: {
			if (IS_NUMBER(peek(0))) {
				Value num = pop();
				num.as.num--;
				push(num);
			}
			else return runtimeError("Operands must be two numbers, got %s and number.", valueTypeToStr(peek(0)).c_str());
			break;
		}
#pragma endregion

#pragma region Binary that returns bools
		case OP_EQUAL: {
			Value b = pop();
			Value a = pop();
			push(BOOL_VAL(valuesEqual(a, b)));
			break;
		}
		case OP_NOT_EQUAL: {
			Value b = pop();
			Value a = pop();
			push(BOOL_VAL(!valuesEqual(a, b)));
			break;
		}
		case OP_GREATER: BINARY_OP(BOOL_VAL, > ); break;
		case OP_LESS: {
			Value a = peek(1);
			Value b = peek(0);
			BINARY_OP(BOOL_VAL, < ); break; }
		case OP_GREATER_EQUAL: {
			//Have to do this because of floating point comparisons
			if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
				return runtimeError("Operands must be two numbers, got %s and %s.",
					valueTypeToStr(peek(1)).c_str(), valueTypeToStr(peek(0)).c_str());
			}
			double b = AS_NUMBER(pop());
			double a = AS_NUMBER(pop());
			if (a > b || FLOAT_EQ(a, b)) push(BOOL_VAL(true));
			else push(BOOL_VAL(false));
			break;
		}
		case OP_LESS_EQUAL: {
			if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
				return runtimeError("Operands must be two numbers, got %s and %s.",
					valueTypeToStr(peek(1)).c_str(), valueTypeToStr(peek(0)).c_str());
			}
			double b = AS_NUMBER(pop());
			double a = AS_NUMBER(pop());
			if (a < b || FLOAT_EQ(a, b)) push(BOOL_VAL(true));
			else push(BOOL_VAL(false));
			break;
		}
#pragma endregion

#pragma region Statements and vars
		case OP_PRINT: {
			printValue(pop());
			std::cout << "\n";
			break;
		}

		case OP_TO_STRING: {
			if (!pushValToStr(this, pop())) return RUNTIME_ERROR;
			//in case pushValToStr encountered a instance of a class that has defined a toString method
			//we need to update the frame pointer
			frame = &frames[frameCount - 1];
			break;
		}

		case OP_DEFINE_GLOBAL: {
			objString* name = READ_STRING();
			VM->curModule->vars.set(name, peek(0));
			pop();
			break;
		}
		case OP_DEFINE_GLOBAL_LONG: {
			objString* name = READ_STRING_LONG();
			VM->curModule->vars.set(name, peek(0));
			pop();
			break;
		}

		case OP_GET_GLOBAL: {
			objString* name = READ_STRING();
			objModule* mod = AS_MODULE(READ_CONSTANT());
			Value value;
			if (!mod->vars.get(name, &value)) {
				if (!VM->globals.get(name, &value)) return runtimeError("Undefined variable '%s'.", name->str);
			}
			push(value);
			break;
		}
		case OP_GET_GLOBAL_LONG: {
			objString* name = READ_STRING_LONG();
			objModule* mod = AS_MODULE(READ_CONSTANT_LONG());
			Value value;
			if (!mod->vars.get(name, &value)) {
				if (!VM->globals.get(name, &value)) return runtimeError("Undefined variable '%s'.", name->str);
			}
			push(value);
			break;
		}

		case OP_SET_GLOBAL: {
			objString* name = READ_STRING();
			objModule* mod = AS_MODULE(READ_CONSTANT());
			if (mod->vars.set(name, peek(0))) {
				mod->vars.del(name);
				return runtimeError("Undefined variable '%s'.", name->str);
			}
			break;
		}
		case OP_SET_GLOBAL_LONG: {
			objString* name = READ_STRING_LONG();
			objModule* mod = AS_MODULE(READ_CONSTANT_LONG());
			if (mod->vars.set(name, peek(0))) {
				mod->vars.del(name);
				return runtimeError("Undefined variable '%s'.", name->str);
			}
			break;
		}

		case OP_GET_LOCAL: {
			uint8_t slot = READ_BYTE();
			push(frame->slots[slot]);
			break;
		}

		case OP_SET_LOCAL: {
			uint8_t slot = READ_BYTE();
			frame->slots[slot] = peek(0);
			break;
		}
						 //upvals[slot]->location can be a pointer to either a stack slot, or a closed upvalue in a functions closure
		case OP_GET_UPVALUE: {
			uint8_t slot = READ_BYTE();
			push(*frame->closure->upvals[slot]->location);
			break;
		}

		case OP_SET_UPVALUE: {
			uint8_t slot = READ_BYTE();
			*frame->closure->upvals[slot]->location = peek(0);
			break;
		}

		case OP_CLOSE_UPVALUE: {
			closeUpvalues(stackTop - 1);
			pop();
			break;
		}
#pragma endregion

#pragma region Control flow
		case OP_JUMP_IF_TRUE: {
			uint16_t offset = READ_SHORT();
			if (!isFalsey(peek(0))) frame->ip += offset;
			break;
		}

		case OP_JUMP_IF_FALSE: {
			uint16_t offset = READ_SHORT();
			if (isFalsey(peek(0))) frame->ip += offset;
			break;
		}

		case OP_JUMP_IF_FALSE_POP: {
			uint16_t offset = READ_SHORT();
			if (isFalsey(pop())) frame->ip += offset;
			break;
		}

		case OP_JUMP: {
			uint16_t offset = READ_SHORT();
			frame->ip += offset;
			break;
		}

		case OP_LOOP: {
			uint16_t offset = READ_SHORT();
			frame->ip -= offset;
			break;
		}

		case OP_JUMP_POPN: {
			uint16_t toPop = READ_SHORT();
			int i = 0;
			while (i < toPop) {
				pop();
				i++;
			}
			uint16_t offset = READ_SHORT();
			frame->ip += offset;
			break;
		}

		case OP_SWITCH: {
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
		case OP_CALL: {
			//how many values are on the stack right now
			int argCount = READ_BYTE();
			if (!callValue(peek(argCount), argCount)) {
				return RUNTIME_ERROR;
			}
			//if the call is succesful, there is a new call frame, so we need to update the pointer
			frame = &frames[frameCount - 1];
			break;
		}

		case OP_RETURN: {
			Value result = pop();
			closeUpvalues(frame->slots);
			frameCount--;
			//if we're returning from the implicit funcition
			if (frameCount == 0) {
				Value val = pop();
				//implcit yielding once a fiber has finished running through all it's code
				if (prevFiber != nullptr) {
					prevFiber->transferValue(result);
					VM->switchToFiber(prevFiber);
					//overwrites the paused state set by switchToFiber, this flag means that if we try to run this fiber again we get nil back
					state = fiberState::FINSIHED;
					//returns paused in order to switch to the previous fiber
					return interpretResult::INTERPRETER_PAUSED;
				}
				return interpretResult::INTERPRETER_OK;
			}

			stackTop = frame->slots;
			push(result);
			//if the call is succesful, there is a new call frame, so we need to update the pointer
			frame = &frames[frameCount - 1];
			break;
		}

		case OP_CLOSURE: {
			//doing this to avoid cached pointers
			objClosure* closure = new objClosure(AS_FUNCTION(READ_CONSTANT()));
			push(OBJ_VAL(closure));
			for (int i = 0; i < closure->upvals.size(); i++) {
				uint8_t isLocal = READ_BYTE();
				uint8_t index = READ_BYTE();
				if (isLocal) {
					closure->upvals[i] = captureUpvalue(frame->slots + index);
				}
				else {
					closure->upvals[i] = frame->closure->upvals[index];
				}
				//this serves to satisfy the GC, since we can't have any cached pointers when we collect
				closure = AS_CLOSURE(peek(0));
			}
			break;
		}
		case OP_CLOSURE_LONG: {
			//doing this to avoid cached pointers
			objClosure* closure = new objClosure(AS_FUNCTION(READ_CONSTANT_LONG()));
			push(OBJ_VAL(closure));
			for (int i = 0; i < closure->upvals.size(); i++) {
				uint8_t isLocal = READ_BYTE();
				uint8_t index = READ_BYTE();
				if (isLocal) {
					closure->upvals[i] = captureUpvalue(frame->slots + index);
				}
				else {
					closure->upvals[i] = frame->closure->upvals[index];
				}
				//this serves to satisfy the GC, since we can't have any cached pointers when we collect
				closure = AS_CLOSURE(peek(0));
			}
			break;
		}
#pragma endregion

#pragma region Objects, arrays and maps
		case OP_CREATE_ARRAY: {
			uInt64 size = READ_BYTE();
			uInt64 i = 0;
			objArray* arr = new objArray(size);
			while (i < size) {
				//size-i to because the values on the stack are in reverse order compared to how they're supposed to be in a array
				Value val = pop();
				//if numOfHeapPtr is 0 we don't trace or update the array when garbage collecting
				if (IS_OBJ(val)) arr->numOfHeapPtr++;

				arr->values[size - i - 1] = val;
				i++;
			}
			push(OBJ_VAL(arr));
			break;
		}

		case OP_GET: {
			//structs and objects also get their own OP_GET_PROPERTY operator for access using '.'
			//use peek because in case this is a get call to a instance that has a defined "access" method
			//we want to use these 2 values as args and receiver
			Value field = peek(0);
			Value callee = peek(1);
			if (!IS_ARRAY(callee) && !IS_INSTANCE(callee))
				return runtimeError("Expected a array or struct, got %s.", valueTypeToStr(callee).c_str());

			switch (AS_OBJ(callee)->type) {
			case OBJ_ARRAY: {
				//we don't pop above because this might be a get call to a class that has a defined access method
				//but in order to maintain stack balance we need to get rid of these 2 vars if this is not a get call to a access method
				pop();
				pop();
				if (!IS_NUMBER(field)) return runtimeError("Index must be a number, got %s.", valueTypeToStr(field).c_str());
				double index = AS_NUMBER(field);
				objArray* arr = AS_ARRAY(callee);
				//Trying to access a variable using a float is a error
				if ((uInt64)index != index) return runtimeError("Expected interger, got float.");
				if (index < 0 || index > arr->values.count() - 1)
					return runtimeError("Index %d outside of range [0, %d].", (uInt64)index, AS_ARRAY(callee)->values.count() - 1);

				push(arr->values[(uInt64)index]);
				break;
			}
			case OBJ_INSTANCE: {
				//check if this instance has a access method, and if it does it transfer control over to the user defined method
				//the 'field' value becomes the argument for the method 
				objString* temp = copyString("access", 6);
				if (!invoke(temp, 1)) {
					return RUNTIME_ERROR;
				}
				else {
					frame = &frames[frameCount - 1];
					break;
				}
				//pop if this instance doesn't contain a access method to maintain stack balance
				pop();
				pop();
				if (!IS_STRING(field)) return runtimeError("Expected a string for field name, got %s.", valueTypeToStr(field).c_str());

				objInstance* instance = AS_INSTANCE(callee);
				objString* name = AS_STRING(field);
				Value value;

				if (instance->table.get(name, &value)) {
					push(value);
					break;
				}

				if (instance->klass && bindMethod(instance->klass, name)) break;
				//if the field doesn't exist, we push nil as a sentinel value, since this is not considered a runtime error
				push(NIL_VAL());
				break;
			}
			}
			break;
		}

		case OP_SET: {
			//structs and objects also get their own OP_SET_PROPERTY operator for setting using '.'
			Value val = peek(0);
			Value field = peek(1);
			Value callee = peek(2);
			//if we encounter a user defined set expr, we will want to exit early and not pop all of the above values as they will be used as args
			bool earlyExit = false;

			if (!IS_ARRAY(callee) && !IS_INSTANCE(callee))
				return runtimeError("Expected a array or struct, got %s.", valueTypeToStr(callee).c_str());

			switch (AS_OBJ(callee)->type) {
			case OBJ_ARRAY: {
				objArray* arr = AS_ARRAY(callee);

				if (!IS_NUMBER(field)) return runtimeError("Index has to be a number, got %s.", valueTypeToStr(field).c_str());
				double index = AS_NUMBER(field);
				//accessing array with a float is a error
				if (index != (uInt64)index) return runtimeError("Index has to be a integer.");
				if (index < 0 || index > arr->values.count() - 1)
					return runtimeError("Index %d outside of range [0, %d].", (uInt64)index, arr->values.count() - 1);

				//if numOfHeapPtr is 0 we don't trace or update the array when garbage collecting
				if (IS_OBJ(val)) arr->numOfHeapPtr++;
				else if (IS_OBJ(arr->values[index])) arr->numOfHeapPtr--;
				arr->values[(uInt64)index] = val;
				break;
			}
			case OBJ_INSTANCE: {
				//check if this instance has a defined set method, if it does, transfer control to it and set the field and value as args to the method
				objString* temp = copyString("set", 3);
				if (!invoke(temp, 2)) {
					return RUNTIME_ERROR;
				}
				else {
					frame = &frames[frameCount - 1];
					earlyExit = true;
					break;
				}
				//if the instance doesn't have a defined set method, proceed as normal
				if (!IS_STRING(field)) return runtimeError("Expected a string for field name, got %s.", valueTypeToStr(field).c_str());

				objInstance* instance = AS_INSTANCE(callee);
				objString* str = AS_STRING(field);
				//settting will always succeed, and we don't care if we're overriding an existing field, or creating a new one
				instance->table.set(str, val);
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

		case OP_CLASS: {
			push(OBJ_VAL(new objClass(READ_STRING_LONG())));
			break;
		}

		case OP_GET_PROPERTY: {
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
		case OP_GET_PROPERTY_LONG: {
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

		case OP_SET_PROPERTY: {
			if (!IS_INSTANCE(peek(1))) {
				return runtimeError("Only instances/structs have properties, got %s.", valueTypeToStr(peek(1)).c_str());
			}

			objInstance* instance = AS_INSTANCE(peek(1));
			//we don't care if we're overriding or creating a new field
			instance->table.set(READ_STRING(), peek(0));

			Value value = pop();
			pop();
			push(value);
			break;
		}
		case OP_SET_PROPERTY_LONG: {
			if (!IS_INSTANCE(peek(1))) {
				return runtimeError("Only instances/structs have properties, got %s.", valueTypeToStr(peek(1)).c_str());
			}

			objInstance* instance = AS_INSTANCE(peek(1));
			//we don't care if we're overriding or creating a new field
			instance->table.set(READ_STRING_LONG(), peek(0));

			Value value = pop();
			pop();
			push(value);
			break;
		}

		case OP_CREATE_STRUCT: {
			int numOfFields = READ_BYTE();

			//passing null instead of class signals to the VM that this is a struct, and not a instance of a class
			objInstance* inst = new objInstance(nullptr);

			//the compiler emits the fields in reverse order, so we can loop through them normally and pop the values on the stack
			for (int i = 0; i < numOfFields; i++) {
				objString* name = READ_STRING();
				inst->table.set(name, pop());
			}
			push(OBJ_VAL(inst));
			break;
		}
		case OP_CREATE_STRUCT_LONG: {
			int numOfFields = READ_BYTE();

			//passing null instead of class signals to the VM that this is a struct, and not a instance of a class
			objInstance* inst = new objInstance(nullptr);

			//the compiler emits the fields in reverse order, so we can loop through them normally and pop the values on the stack
			for (int i = 0; i < numOfFields; i++) {
				objString* name = READ_STRING_LONG();
				inst->table.set(name, pop());
			}
			push(OBJ_VAL(inst));
			break;
		}

		case OP_METHOD: {
			//class that this method binds too
			defineMethod(READ_STRING_LONG());
			break;
		}

		case OP_INVOKE: {
			//gets the method and calls it immediatelly, without converting it to a objBoundMethod
			objString* method = READ_STRING();
			int argCount = READ_BYTE();
			if (!invoke(method, argCount)) {
				return RUNTIME_ERROR;
			}
			frame = &frames[frameCount - 1];
			break;
		}
		case OP_INVOKE_LONG: {
			//gets the method and calls it immediatelly, without converting it to a objBoundMethod
			objString* method = READ_STRING_LONG();
			int argCount = READ_BYTE();
			if (!invoke(method, argCount)) {
				return RUNTIME_ERROR;
			}
			frame = &frames[frameCount - 1];
			break;
		}

		case OP_INHERIT: {
			Value superclass = peek(1);
			if (!IS_CLASS(superclass)) {
				return runtimeError("Superclass must be a class, got %s.", valueTypeToStr(superclass).c_str());
			}
			objClass* subclass = AS_CLASS(peek(0));
			//copy down inheritance
			subclass->methods.tableAddAll(&subclass->methods);
			break;
		}

		case OP_GET_SUPER: {
			//super is ALWAYS followed by a field and is a call expr
			objString* name = READ_STRING();
			objClass* superclass = AS_CLASS(pop());

			if (!bindMethod(superclass, name)) {
				return RUNTIME_ERROR;
			}
			break;
		}
		case OP_GET_SUPER_LONG: {
			//super is ALWAYS followed by a field and is a call expr
			objString* name = READ_STRING_LONG();
			objClass* superclass = AS_CLASS(pop());

			if (!bindMethod(superclass, name)) {
				return RUNTIME_ERROR;
			}
			break;
		}

		case OP_SUPER_INVOKE: {
			//works same as OP_INVOKE, but uses invokeFromClass() to specify the superclass
			objString* method = READ_STRING();
			int argCount = READ_BYTE();
			objClass* superclass = AS_CLASS(pop());

			if (!invokeFromClass(superclass, method, argCount)) {
				return RUNTIME_ERROR;
			}
			frame = &frames[frameCount - 1];
			break;
		}
		case OP_SUPER_INVOKE_LONG: {
			//works same as OP_INVOKE, but uses invokeFromClass() to specify the superclass
			objString* method = READ_STRING_LONG();
			int argCount = READ_BYTE();
			objClass* superclass = AS_CLASS(pop());

			if (!invokeFromClass(superclass, method, argCount)) {
				return RUNTIME_ERROR;
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