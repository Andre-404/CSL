#pragma once
#include "../common.h"

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
	IMPORT, MACRO, EXPORT,
	YIELD, FIBER, RUN,

	NEWLINE, ERROR, TOKEN_EOF
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
	string getStr() {
		uInt64 start = sourceFile->lines[line - 1] + column;
		return sourceFile->sourceFile.substr(start, length);
	}
};

struct Token {
	TokenType type;
	Span str;
	//for things like synthetic tokens and expanded macros
	long long line;
	bool partOfMacro;
	Span macro;

	bool isSynthetic;
	const char* ptr;
	//default constructor
	Token() {
		isSynthetic = false;
		ptr = nullptr;
		line = -1;
		partOfMacro = false;
		type = TokenType::LEFT_PAREN;
	}
	//construct a token from source file string data
	Token(Span _str, TokenType _type) {
		isSynthetic = false;
		line = _str.line;
		ptr = nullptr;
		str = _str;
		type = _type;
		partOfMacro = false;
	}
	//construct a token which doesn't appear in the source file(eg. splitting a += b into a = a + b, where '+' is synthetic)
	Token(const char* _ptr, uInt64 _line, TokenType _type) {
		ptr = _ptr;
		isSynthetic = true;
		type = _type;
		line = _line;
		partOfMacro = false;
	}
	string getLexeme() {
		if (isSynthetic) return string(ptr);
		return str.getStr();
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