#include "codegenDefs.h"
#include "../ErrorHandling/errorHandler.h"
#include "../Objects/objects.h"
#include "../MemoryManagment/garbageCollector.h"
#include "../DebugPrinting/BytecodePrinter.h"
#include <format>

using namespace object;

Chunk::Chunk() {
}

void Chunk::writeData(uint8_t opCode, uInt line, byte fileIndex) {
	code.push(opCode);
	if (lines.size() == 0) {
		lines.push(codeLine(line, fileIndex));
		return;
	}
	if (lines[lines.size() - 1].line == line) return;
	//if we're on a new line, mark the end of the bytecode for this line
	//when looking up the line of code for a particular OP we check if it's position in 'code' is less than .end of a line
	lines[lines.size() - 1].end = code.size() - 1;
	lines.push(codeLine(line, fileIndex));
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
		if (constants[i].equals(val)) return i;
	}
	uInt size = constants.size();
	constants.push(val);
	return size;
}


bool Value::equals(Value other) {
	if (type != other.type) return false;
	switch (type) {
	case ValueType::NUM: return FLOAT_EQ(asNum(), other.asNum());
	case ValueType::BOOL: return asBool() == other.asBool();
	case ValueType::OBJ: return asObj() == other.asObj();
	case ValueType::NIL: return true;
	}
}

string valueToStr(Value* val) {
	switch (val->type) {
	case ValueType::BOOL:
		return val->asBool() ? "true" : "false";
		break;
	case ValueType::NIL: return "nil"; break;
	case ValueType::NUM: {
		double num = val->asNum();
		int prec = (num == static_cast<int>(num)) ? 0 : 5;
		return std::to_string(num).substr(0, std::to_string(num).find(".") + prec);
		break;
	}
	case ValueType::OBJ: return val->asObj()->toString(); break;
	default:
		std::cout << "Error printing object";
		return "";
	}
}

void Value::print() {
	std::cout << valueToStr(this);
}

bool Value::isString() {
	return isObj() && asObj()->type == ObjType::STRING;
}
bool Value::isFunction() {
	return isObj() && asObj()->type == ObjType::FUNC;
}
bool Value::isNativeFn() {
	return isObj() && asObj()->type == ObjType::NATIVE;
}
bool Value::isArray() {
	return isObj() && asObj()->type == ObjType::ARRAY;
}
bool Value::isClosure() {
	return isObj() && asObj()->type == ObjType::CLOSURE;
}
bool Value::isClass() {
	return isObj() && asObj()->type == ObjType::CLASS;
}
bool Value::isInstance() {
	return isObj() && asObj()->type == ObjType::INSTANCE;
}
bool Value::isBoundMethod() {
	return isObj() && asObj()->type == ObjType::BOUND_METHOD;
}
bool Value::isThread() {
	return isObj() && asObj()->type == ObjType::THREAD;
}
bool Value::isFile() {
	return isObj() && asObj()->type == ObjType::FILE;
}

object::ObjString* Value::asString() {
	return dynamic_cast<ObjString*>(asObj());
}
object::ObjFunc* Value::asFunction() {
	return dynamic_cast<ObjFunc*>(asObj());
}
object::ObjNativeFunc* Value::asNativeFn() {
	return dynamic_cast<ObjNativeFunc*>(asObj());
}
object::ObjArray* Value::asArray() {
	return dynamic_cast<ObjArray*>(asObj());
}
object::ObjClosure* Value::asClosure() {
	return dynamic_cast<ObjClosure*>(asObj());
}
object::ObjClass* Value::asClass() {
	return dynamic_cast<ObjClass*>(asObj());
}
object::ObjInstance* Value::asInstance() {
	return dynamic_cast<ObjInstance*>(asObj());
}
object::ObjBoundMethod* Value::asBoundMethod() {
	return dynamic_cast<ObjBoundMethod*>(asObj());
}
object::ObjThread* Value::asThread() {
	return dynamic_cast<ObjThread*>(asObj());
}
object::ObjFile* Value::asFile() {
	return dynamic_cast<ObjFile*>(asObj());
}

void Value::mark() {
	if (isObj()) memory::gc.markObj(asObj());
}

void Value::updatePtr() {
	if (isObj()) as.object = reinterpret_cast<Obj*>(as.object->moveTo);
}