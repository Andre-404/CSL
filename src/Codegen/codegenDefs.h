#pragma once
#include "../DataStructures/gcArray.h"

namespace object {
	class Obj;

	class ObjString;

	class ObjArray;

	class ObjFunc;

	class ObjNativeFunc;

	class ObjUpval;

	class ObjClosure;

	class ObjClass;

	class ObjBoundMethod;

	class ObjInstance;

	class ObjThread;

	class ObjFile;
}

enum class ValueType {
	NUM,
	BOOL,
	OBJ,
	NIL
};

struct Value {
	ValueType type;
	union {
		double number;
		bool boolean;
		object::Obj* object;
	} as;
	Value() {
		type = ValueType::NIL;
		as.object = nullptr;
	}

	Value(double num) {
		type = ValueType::NUM;
		as.number = num;
	}

	Value(bool _bool) {
		type = ValueType::BOOL;
		as.boolean = _bool;
	}

	Value(object::Obj* _object) {
		type = ValueType::OBJ;
		as.object = _object;
	}
	
	static Value nil() {
		return Value();
	}

	bool equals(Value other) {
		return false;
	}

	#pragma region Helpers
	bool isBool() { return type == ValueType::BOOL; };
	bool isNumber() { return type == ValueType::NUM; };
	bool isNil() { return type == ValueType::NIL; };
	bool isObj() { return type == ValueType::OBJ; };

	bool asBool() { return as.boolean; }
	double asNum() { return as.number; }
	object::Obj* asObj() { return as.object; }

	bool isString();
	bool isFunction();
	bool isNativeFn();
	bool isArray();
	bool isClosure();
	bool isClass();
	bool isInstance();
	bool isBoundMethod();
	bool isThread();
	bool isFile();

	object::ObjString* asString();
	object::ObjFunc* asFunction();
	object::ObjNativeFunc* asNativeFn();
	object::ObjArray* asArray();
	object::ObjClosure* asClosure();
	object::ObjClass* asClass();
	object::ObjInstance* asInstance();
	object::ObjBoundMethod* asBoundMethod();
	object::ObjThread* asThread();
	object::ObjFile* asFile();

	void mark();
	void updatePtr();
	#pragma endregion
};

enum class OpCode {
	//Helpers
	POP,
	POPN,//arg: 8-bit num
	//constants
	CONSTANT,//arg: 8-bit constant index
	CONSTANT_LONG,//arg: 8-bit constant index
	NIL,
	TRUE,
	FALSE,
	//unary
	NEGATE,
	NOT,
	BIN_NOT,
	//binary
	BITWISE_XOR,
	BITWISE_OR,
	BITWISE_AND,
	ADD,
	SUBTRACT,
	MULTIPLY,
	DIVIDE,
	MOD,
	BITSHIFT_LEFT,
	BITSHIFT_RIGHT,
	//optimizations, if the rhs of one of these binary ops is a integer thats smaller than 256
	//it's directly inserted into the bytecode instead of having a new constant
	ADD_INT,//arg: 8-bit num
	SUBTRACT_INT,//arg: 8-bit num
	DIVIDE_INT,//arg: 8-bit num
	MULTIPLY_INT,//arg: 8-bit num
	//comparisons and equality
	EQUAL,
	NOT_EQUAL,
	GREATER,
	GREATER_EQUAL,
	LESS,
	LESS_EQUAL,
	//temporary
	PRINT,
	//Variables
	//all module level variables(including class and function declarations) are treated as global variables
	//compiler adds <module name>:: to the front of the variable name to make it distinct
	DEFINE_GLOBAL,//arg: 8-bit constant index
	DEFINE_GLOBAL_LONG,//arg: 16-bit constant index
	GET_GLOBAL,//arg: 8-bit constant index
	GET_GLOBAL_LONG,//arg: 16-bit constant index
	SET_GLOBAL,//arg: 8-bit constant index
	SET_GLOBAL_LONG,//arg: 16-bit constant index

	GET_LOCAL,//arg: 8-bit stack position
	SET_LOCAL,//arg: 8-bit stack position
	GET_UPVALUE,//arg: 8-bit upval position
	SET_UPVALUE,//arg: 8-bit upval position
	CLOSE_UPVALUE,
	//Arrays
	CREATE_ARRAY,//arg: 8-bit array size
	//get and set is used by both arrays and instances/structs, since struct.field is just syntax sugar for struct["field"] that
	//gets optimized to use GET_PROPERTY
	GET,
	SET,
	//control flow
	JUMP,//arg: 16-bit jump offset
	JUMP_IF_FALSE,//arg: 16-bit jump offset
	JUMP_IF_TRUE,//arg: 16-bit jump offset
	JUMP_IF_FALSE_POP,//arg: 16-bit jump offset
	LOOP,//arg: 16-bit jump offset(gets negated)
	JUMP_POPN, //arg: 16-bit jump offset

	//Functions
	CALL,//arg: 8-bit argument count
	RETURN,
	CLOSURE,//arg: 8-bit ObjFunction constant index
	CLOSURE_LONG,//arg: 16-bit ObjFunction constant index

	//OOP
	CLASS,//arg: 16-bit ObjString constant index
	GET_PROPERTY,//arg: 8-bit ObjString constant index
	GET_PROPERTY_LONG,//arg: 16-bit ObjString constant index
	SET_PROPERTY,//arg: 8-bit ObjString constant index
	SET_PROPERTY_LONG,//arg: 16-bit ObjString constant index
	CREATE_STRUCT,//arg: 8-bit number of fields
	CREATE_STRUCT_LONG,//arg: 8-bit number of fields
	METHOD,//arg: 16-bit ObjString constant index
	INVOKE,//arg: 8-bit ObjString constant index, 8-bit argument count
	INVOKE_LONG,//arg: 16-bit ObjString constant index, 8-bit argument count
	INHERIT,
	GET_SUPER,//arg: 8-bit ObjString constant index
	GET_SUPER_LONG,//arg: 16-bit ObjString constant index
	SUPER_INVOKE,//arg: 8-bit ObjString constant index, 8-bit argument count
	SUPER_INVOKE_LONG,//arg: 16-bit ObjString constant index, 8-bit argument count

	//fibers
	THREAD_RUN,
	THREAD_YIELD,
};


struct codeLine {
	uInt64 end;
	uInt64 line;
	string name;

	codeLine() {
		line = 0;
		end = 0;
		name = "";
	}
	codeLine(uInt64 _line, string& _name) {
		line = _line;
		name = _name;
	}
};

class Chunk {
public:
	ManagedArray<codeLine> lines;

	ManagedArray<uint8_t> code;
	ManagedArray<Value> constants;
	Chunk();
	void writeData(uint8_t opCode, uInt line, string& name);
	codeLine getLine(uInt offset);
	void disassemble(string name);
	uInt addConstant(Value val);
};

#define FRAMES_MAX 256
#define STACK_MAX (FRAMES_MAX * 256)

struct CallFrame {
	object::ObjClosure* closure;
	uInt64 ip;
	Value* slots;
	CallFrame() : closure(nullptr), ip(0), slots(nullptr) {};
};

enum class RuntimeResult {
	OK,
	RUNTIME_ERROR,
	PAUSED,
};

enum class ThreadState {
	NOT_STARTED,
	RUNNING,
	PAUSED,
	FINSIHED
};