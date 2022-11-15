#include "common.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

string readFile(char* path) {

	std::filesystem::path p(path);
	if (std::filesystem::exists(p)) {
		std::stringstream ss;
		uInt64 size = std::filesystem::file_size(p);
		std::ifstream is(path);
		ss << is.rdbuf();
		is.close();
		return ss.str();
	}
	else {
		std::cout << "Couldn't open file " << path << "\n";
		return "";
	}
}
string readFile(const char* path) {
	return readFile((char*)path);
}
string readFile(string& path) {
	return readFile(path.c_str());
}