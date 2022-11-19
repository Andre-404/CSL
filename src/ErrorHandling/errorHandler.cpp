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

	underlineSymbol(token.str);
	std::cout << "\n";
}

namespace errorHandler {
	bool hadError = false;
	namespace {
		struct SystemError {
			string errorText;

			SystemError(string _errorText) {
				errorText = _errorText;
			}
		};
		struct CompileTimeError {
			string errorText;
			File* origin;
			Token token;

			CompileTimeError(string _errorText, File* _origin, Token _token) {
				errorText = _errorText;
				origin = _origin;
				token = _token;
			}
		};
		struct RuntimeError {
			string errorText;
			string funcName;
			CSLModule* origin;

			RuntimeError(string _errorText, CSLModule* _origin, string _funcName) {
				errorText = _errorText;
				origin = _origin;
				funcName = _funcName;
			}
		};

		//errors during preprocessing, building of the AST tree and compiling
		vector<CompileTimeError> compileErrors;
		//stack trace when a runtime error occurs
		vector<RuntimeError> runtimeErrors;
		//system level errors(eg. not being able to access a file)
		vector<SystemError> systemErrors;
	}

	void showCompileErrors() {
		for (CompileTimeError error : compileErrors) {
			report(error.origin, error.token, error.errorText);
		}
	}

	void showRuntimeErrors() {
		//TODO: implement this when you get to stack tracing
	}
	void showSystemErrors() {
		for (SystemError error : systemErrors) {
			std::cout << "System error: " << error.errorText << "\n";
		}
	}

	void addCompileError(string msg, Token token) {
		compileErrors.push_back(CompileTimeError(msg, token.str.sourceFile, token));
	}
	void addRuntimeError(string msg, string funcName, CSLModule* origin) {
		runtimeErrors.push_back(RuntimeError(msg, origin, funcName));
	}
	void addSystemError(string msg) {
		systemErrors.push_back(SystemError(msg));
	}
}