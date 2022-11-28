#pragma once
#include "../common.h"
#include "../modulesDefs.h"

namespace errorHandler {
	void showCompileErrors();
	void showRuntimeErrors();
	void showSystemErrors();

	void addCompileError(string msg, Token token);
	void addRuntimeError(string msg, string funcName, CSLModule* origin);
	void addSystemError(string msg);

	class SystemException {

	};
}