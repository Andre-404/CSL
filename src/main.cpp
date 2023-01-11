#include <iostream>
#include <filesystem>
#include "Preprocessing/preprocessor.h"
#include "ErrorHandling/errorHandler.h"
#include "Parsing/parser.h"
#include "Codegen/compiler.h"


int main(int argc, char* argv[]) {
	preprocessing::Preprocessor p;
	p.preprocessProject("C:\\Temp\\main.csl");
	vector<CSLModule*> modules = p.getSortedUnits();
	AST::Parser pa;
	pa.parse(modules);
	compileCore::Compiler c(modules);
	errorHandler::showCompileErrors();
	return 0;
}