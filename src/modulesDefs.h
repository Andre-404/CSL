#pragma once
#include "common.h"
#include <memory>

enum class TokenType {
	// Single-character tokens.
	LEFT_PAREN, RIGHT_PAREN,
	LEFT_BRACE, RIGHT_BRACE,
	LEFT_BRACKET, RIGHT_BRACKET,
	COMMA, DOT, MINUS, PLUS,
	SEMICOLON, SLASH, STAR, PERCENTAGE,
	QUESTIONMARK, COLON, TILDA,
	// One or two character tokens.
	BITWISE_AND, BITWISE_OR, BITWISE_XOR,
	BANG, BANG_EQUAL,
	EQUAL, EQUAL_EQUAL, PLUS_EQUAL, MINUS_EQUAL, STAR_EQUAL, SLASH_EQUAL,
	PERCENTAGE_EQUAL, BITWISE_AND_EQUAL, BITWISE_OR_EQUAL, BITWISE_XOR_EQUAL,
	GREATER, GREATER_EQUAL,
	LESS, LESS_EQUAL,
	BITSHIFT_LEFT, BITSHIFT_RIGHT,
	INCREMENT, DECREMENT, DOUBLE_COLON,
	// Literals.
	IDENTIFIER, STRING, NUMBER,
	// Keywords.
	AND, OR,
	NIL, FALSE, TRUE,
	IF, ELSE,
	FUNC, RETURN,
	WHILE, FOR, CONTINUE, BREAK,
	CLASS, THIS, SUPER,
	SWITCH, CASE, DEFAULT,
	PRINT, VAR,
	IMPORT, ADDMACRO, REMOVEMACRO, EXPORT,

	WHITESPACE, NEWLINE, ERROR, TOKEN_EOF
};

struct File {
	//file name
	string name;
	string sourceFile;
	//number that represends start of each line in the source string
	std::vector<uInt> lines;
	File(string& src, string& _name) : sourceFile(src), name(_name) {};
	File() {}
};

//span of characters in a source file of code
struct Span {
	//sourceFile.lines[line - 1] + column is the start of the string
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
	string getStr() const {
		uInt64 start = sourceFile->lines[line - 1] + column;
		return sourceFile->sourceFile.substr(start, length);
	}
};

struct Token {
	TokenType type;
	Span str;

	//for things like synthetic tokens and expanded macros
	bool isSynthetic;
	std::shared_ptr<Token> ptr;
	//default constructor
	Token() {
		isSynthetic = false;
		ptr = nullptr;
		type = TokenType::LEFT_PAREN;
	}
	//construct a token from source file string data
	Token(Span _str, TokenType _type) {
		isSynthetic = false;
		ptr = nullptr;
		str = _str;
		type = _type;
	}
	//construct a token which doesn't appear in the source file(eg. splitting a += b into a = a + b, where '+' is synthetic)
	Token(TokenType _type, Token parentToken) {
		ptr = std::shared_ptr<Token>(new Token(parentToken));
		isSynthetic = true;
		type = _type;
	}
	string getLexeme() const {
		if (type == TokenType::ERROR) return "Unexpected character.";
		else if (isSynthetic) return "synthetic token";
		return str.getStr();
	}

	void addParentToken(Token token) {
		ptr = std::shared_ptr<Token>(new Token(token));
	}
};

struct CSLModule {
	File* file;
	vector<Token> tokens;
	vector<CSLModule*> deps;
	//whether the dependency tree of this module has been resolved, if it hasn't and we try to parse
	//this module again we have a circular dependency and an error will be thrown
	bool resolvedDeps;
	//used for topsort once we have resolved all dependencies 
	bool traversed;

	CSLModule(vector<Token> _tokens, File* _file) {
		tokens = _tokens;
		file = _file;
		resolvedDeps = false;
		traversed = false;
	};
};