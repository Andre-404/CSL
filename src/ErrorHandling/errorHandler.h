#pragma once
#include "../common.h"
#include "../Preprocessing/scanner.h"

struct CompileTimeError {
	string errorText;
	CSLModule* origin;
	Token token;

	CompileTimeError(string _errorText, CSLModule* _origin, Token _token) {
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

class ErrorHandler {
public:
	vector<CompileTimeError> errors;
	vector<RuntimeError> stackTrace;

	void showCompileErrors() {};
	void showRuntimeErrors() {};
	ErrorHandler() {}
};