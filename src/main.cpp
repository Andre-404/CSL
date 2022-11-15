#include <iostream>
#include <filesystem>
#include "Preprocessing/preprocessor.h"
#include "ErrorHandling/errorHandler.h"


int main(int argc, char* argv[]) {
	ErrorHandler h;
	preprocessing::Preprocessor p(h);
	bool a = p.preprocessProject("C:\\Temp\\main.csl");
	h.showCompileErrors();
	return 0;
}