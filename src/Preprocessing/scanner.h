#pragma once
#include "../modulesDefs.h"

namespace preprocessing {

struct Span{
	uInt64 line;
	uInt64 column;
	uInt64 length;

	File* sourceFile;
	Span() {
		line = 0;
		column = 0;
		length = 0;
		sourceFile = nullptr;
	}
	Span(uInt64 _line, uInt64 _column, uInt64 _len, File* _src) : line(_line), column(_column), length(_len), sourceFile(_src) {};
	string getStr() {
		uInt64 start = sourceFile->lines[line - 1] + column;
		return sourceFile->sourceFile.substr(start, length);
	}
};

class Scanner {
	public:
		vector<Token> tokenizeSource(string source, string sourcename);
		File* getFile() { return curFile; }
		Scanner();
	private:
		File* curFile;
		int line;
		int start;
		int current;
		bool hadError;
		vector<Token> tokens;

		Token scanToken();
		Token makeToken(TokenType type);
		Token errorToken(const char* message);

		bool isAtEnd();
		bool isIndexInFile(int index);
		bool match(char expected);
		bool checkKeyword(int keywordOffset, string keyword);
		char advance();
		char peek();
		char peekNext();
		void skipWhitespace();

		Token string_();

		Token number();

		Token identifier();
		TokenType identifierType();

		void reset();
	};
}
