#include <iostream>
#include <filesystem>
#include "Preprocessing/preprocessor.h"
#include "ErrorHandling/errorHandler.h"


int main(int argc, char* argv[]) {
	preprocessing::Preprocessor p;
	bool a = p.preprocessProject("C:\\Temp\\main.csl");
	return 0;
}