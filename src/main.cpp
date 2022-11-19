#include <iostream>
#include <filesystem>
#include "Preprocessing/preprocessor.h"
#include "ErrorHandling/errorHandler.h"
#include "Parsing/parser.h"


int main(int argc, char* argv[]) {
	preprocessing::Preprocessor p;
	p.preprocessProject("C:\\Temp\\main.csl");
	errorHandler::showCompileErrors();
	AST::Parser pa;
	pa.parse(p.getSortedUnits());
	return 0;
}