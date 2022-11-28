#include "codegenDefs.h"
#include "../ErrorHandling/errorHandler.h"
#include <format>

Chunk::Chunk() {

}

void Chunk::writeData(uint8_t opCode, uInt line, string& name) {
	code.push(opCode);
	if (lines.size() == 0) {
		lines.push(codeLine(line, name));
		return;
	}
	if (lines[lines.size() - 1].line == line) return;
	//if we're on a new line, mark the end of the bytecode for this line
	//when looking up the line of code for a particular OP we check if it's position in 'code' is less than .end of a line
	lines[lines.size() - 1].end = code.size() - 1;
	lines.push(codeLine(line, name));
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
	//debug
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