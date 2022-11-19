#include <iostream>
#include <filesystem>
#include "Preprocessing/preprocessor.h"
#include "ErrorHandling/errorHandler.h"


int main(int argc, char* argv[]) {
	preprocessing::Preprocessor p;
	p.preprocessProject("C:\\Temp\\main.csl");
	return 0;
}