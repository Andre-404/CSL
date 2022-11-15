#include "errorHandler.h"
#include "../Preprocessing/scanner.h"
#include <iostream>

//name:line:column: error: msg
//line
//which part
//symbol: token.getLexeme()

const string cyan = "\u001b[38;5;117m";
const string black = "\u001b[0m";
const string red = "\u001b[38;5;196m";
const string yellow = "\u001b[38;5;220m";

void underlineSymbol(Span symbol) {
	File* src = symbol.sourceFile;
	uInt64 lineStart = src->lines[symbol.line - 1];
	uInt64 lineEnd = 0;

	if (src->lines.size() - 1 == symbol.line - 1) lineEnd = src->sourceFile.size();
	else lineEnd = src->lines[symbol.line];

	string line = std::to_string(symbol.line);
	std::cout << yellow + src->name + black + ":" + cyan + line + " | " + black;
	std::cout << src->sourceFile.substr(lineStart, lineEnd - lineStart);

	string temp = "";
	temp.insert(0, src->name.size() + line.size() + 4, ' ');
	uInt64 tempN = 0;
	for (; tempN < symbol.column; tempN++) temp.append(" ");
	for (; tempN < symbol.column + symbol.length; tempN++) temp.append("^");

	std::cout << red + temp + black + "\n";
}

void report(File* src, Token& token, string msg) {
	if (token.type == TokenType::TOKEN_EOF) {
		std::cout << "End of file. \n" << msg;
		return;
	}
	string name = "\u001b[38;5;220m" + src->name + black;
	std::cout << red + "error: " + black + msg + "\n";

	if (token.partOfMacro) {
		underlineSymbol(token.macro);
	}

	underlineSymbol(token.str);
	std::cout << "\n";
}

void ErrorHandler::showCompileErrors() {
	for (CompileTimeError error : errors) {
		report(error.origin->file, error.token, error.errorText);
	}
}
