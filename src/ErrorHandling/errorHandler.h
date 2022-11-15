#pragma once
#include "../common.h"
#include "../modulesDefs.h"

struct SystemError {
	string errorText;

	SystemError(string _errorText) {
		errorText = _errorText;
	}
};

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
	//errors during preprocessing, building of the AST tree and compiling
	vector<CompileTimeError> compileErrors;
	//stack trace when a runtime error occurs
	vector<RuntimeError> runtimeErrors;
	//system level errors(eg. not being able to access a file)
	vector<SystemError> systemErrors;

	void showCompileErrors();
	void showRuntimeErrors() {};
	ErrorHandler() {}

	void addError(CompileTimeError error);
	void addError(RuntimeError error);
	void addError(SystemError error);
};