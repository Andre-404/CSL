#pragma once
#include "../Parsing/ASTDefs.h"

namespace codeFlowAnalysis {

	struct Local {
		AST::VarDecl* var;
		int depth;
	};

	struct CurrentChunkInfo {
		//for closures
		CurrentChunkInfo* enclosing;
		Local locals[256];
		uInt localCount;
		uInt scopeDepth;
		bool hasCapturedLocals;
		CurrentChunkInfo(CurrentChunkInfo* _enclosing);
	};
}