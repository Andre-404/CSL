#include "codegenDefs.h"
#include "../ErrorHandling/errorHandler.h"
#include "../Objects/objects.h"
#include "../MemoryManagment/garbageCollector.h"
#include "../DebugPrinting/BytecodePrinter.h"
#include <format>

using namespace object;

Chunk::Chunk() {}

void Chunk::writeData(uint8_t opCode, uInt line, byte fileIndex) {
	code.push_back(opCode);
	if (lines.size() == 0) {
		lines.push_back(codeLine(line, fileIndex));
		return;
	}
	if (lines[lines.size() - 1].line == line) return;
	//if we're on a new line, mark the end of the bytecode for this line
	//when looking up the line of code for a particular OP we check if it's position in 'code' is less than .end of a line
	lines[lines.size() - 1].end = code.size() - 1;
	lines.push_back(codeLine(line, fileIndex));
}

codeLine Chunk::getLine(uInt offset) {
	for (int i = 0; i < lines.size(); i++) {
		codeLine& line = lines[i];
		if (offset < line.end) return line;
	}
	errorHandler::addSystemError(std::format("Couldn't show line for bytecode at position: {}", offset));
	throw errorHandler::SystemException();
}

void Chunk::disassemble(string name) {
	std::cout << "=======" << name << "=======\n";
	//prints every instruction in chunk
	for (uInt offset = 0; offset < code.size();) {
		offset = disassembleInstruction(this, offset);
	}
}

//adds the constant to the array and returns it's index, which is used in conjuction with OP_CONSTANT
//first checks if this value already exists, this helps keep the constants array small
//returns index of the constant
uInt Chunk::addConstant(Value val) {
	for (uInt i = 0; i < constants.size(); i++) {
		if (constants[i] == val) return i;
	}
	uInt size = constants.size();
	constants.push_back(val);
	return size;
}

string valueToStr(Value& val) {
	switch (val.value.index()) {
	case 0: {
		double num = get<double>(val.value);
		int prec = (num == static_cast<int>(num)) ? 0 : 5;
		return std::to_string(num).substr(0, std::to_string(num).find(".") + prec);
	}
	case 1:
		return get<bool>(val.value) ? "true" : "false";
	case 2: 
		if (get<object::Obj*>(val.value) == nullptr) return "nil";
		return get<object::Obj*>(val.value)->toString(); 
	default:
		std::cout << "Error printing object";
		return "";
	}
}

bool Value::operator== (const Value& other) const {
	return value == other.value;
}

bool Value::operator!=(const Value& other) const
{
	return !(*this == other);
}

void Value::print() {
	std::cout << valueToStr(*this);
}

bool Value::isString() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::STRING;
}
bool Value::isFunction() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::FUNC;
}
bool Value::isNativeFn() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::NATIVE;
}
bool Value::isArray() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::ARRAY;
}
bool Value::isClosure() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::CLOSURE;
}
bool Value::isClass() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::CLASS;
}
bool Value::isInstance() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::INSTANCE;
}
bool Value::isBoundMethod() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::BOUND_METHOD;
}
bool Value::isFile() {
	return isObj() && get<object::Obj*>(value)->type == ObjType::FILE;
}

object::ObjString* Value::asString() {
	return dynamic_cast<ObjString*>(get<object::Obj*>(value));
}

object::ObjFunc* Value::asFunction() {
	return dynamic_cast<ObjFunc*>(get<object::Obj*>(value));
}
object::ObjNativeFunc* Value::asNativeFn() {
	return dynamic_cast<ObjNativeFunc*>(get<object::Obj*>(value));
}
object::ObjArray* Value::asArray() {
	return dynamic_cast<ObjArray*>(get<object::Obj*>(value));
}
object::ObjClosure* Value::asClosure() {
	return dynamic_cast<ObjClosure*>(get<object::Obj*>(value));
}
object::ObjClass* Value::asClass() {
	return dynamic_cast<ObjClass*>(get<object::Obj*>(value));
}
object::ObjInstance* Value::asInstance() {
	return dynamic_cast<ObjInstance*>(get<object::Obj*>(value));
}
object::ObjBoundMethod* Value::asBoundMethod() {
	return dynamic_cast<ObjBoundMethod*>(get<object::Obj*>(value));
}
object::ObjFile* Value::asFile() {
	return dynamic_cast<ObjFile*>(get<object::Obj*>(value));
}

void Value::mark() {
	if (isObj()) memory::gc.markObj(get<object::Obj*>(value));
}

void Value::updatePtr() {
	if (isObj()) value = reinterpret_cast<Obj*>(get<object::Obj*>(value)->moveTo);
}

string Value::typeToStr() {
	switch (value.index()) {
	case 0: return "number";
	case 1: return "bool";
	case 2:
		object::Obj* temp = get<object::Obj*>(value);
		switch (temp->type) {
		case object::ObjType::ARRAY: return "array";
		case object::ObjType::BOUND_METHOD: return "method";
		case object::ObjType::CLASS: return "class " + string(asClass()->name->getString());
		case object::ObjType::CLOSURE: return "function";
		case object::ObjType::FUNC: return "function";
		case object::ObjType::INSTANCE: return asInstance()->klass == nullptr ? "struct" : "instance";
		case object::ObjType::NATIVE: return "native function";
		case object::ObjType::STRING: return "string";
		case object::ObjType::UPVALUE: return "upvalue";
		}
	}
	return "error, couldn't determine type of value";
}