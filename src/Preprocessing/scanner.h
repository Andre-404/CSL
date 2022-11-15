#pragma once
#include "../modulesDefs.h"

namespace preprocessing {

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
		bool match(char expected);
		char advance();
		char peek();
		char peekNext();
		void skipWhitespace();

		Token string_();

		bool isDigit(char c);
		Token number();

		bool isAlpha(char c);
		Token identifier();
		TokenType identifierType();
		TokenType checkKeyword(int start, int length, const char* rest, TokenType type);

		void reset();
	};
}