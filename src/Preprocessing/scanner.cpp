#include "scanner.h"

Scanner::Scanner() {
	line = 0;
	start = 0;
	current = 0;
	hadError = false;
}

vector<Token> Scanner::tokenizeSource(string source, string sourceName) {
	curFile = new File(source, sourceName);
	line = 1;
	start = 0;
	current = start;
	curFile->lines.push_back(0);
	hadError = false;
	tokens.clear();

	if (source != "") {
		Token token = scanToken();
		tokens.push_back(token);
		while (token.type != TokenType::TOKEN_EOF) {
			token = scanToken();
			tokens.push_back(token);
		}
	}
	return tokens;
}

void Scanner::reset() {
	curFile = nullptr;
	line = 0;
	start = 0;
	current = 0;
	hadError = false;
	tokens.clear();
}

#pragma region Helpers
bool Scanner::isAtEnd() {
	return current >= curFile->sourceFile.size();
}

//if matched we consume the token
bool Scanner::match(char expected) {
	if (isAtEnd()) return false;
	if (curFile->sourceFile[current] != expected) return false;
	current++;
	return true;
}

char Scanner::advance() {
	return curFile->sourceFile[current++];
}

Token Scanner::scanToken() {
	skipWhitespace();
	start = current;

	if (isAtEnd()) return makeToken(TokenType::TOKEN_EOF);

	char c = advance();
	//identifiers start with _ or [a-z][A-Z]
	if (isDigit(c)) return number();
	if (isAlpha(c)) return identifier();

	switch (c) {
	case '(': return makeToken(TokenType::LEFT_PAREN);
	case ')': return makeToken(TokenType::RIGHT_PAREN);
	case '{': return makeToken(TokenType::LEFT_BRACE);
	case '}': return makeToken(TokenType::RIGHT_BRACE);
	case '[': return makeToken(TokenType::LEFT_BRACKET);
	case ']': return makeToken(TokenType::RIGHT_BRACKET);
	case ';': return makeToken(TokenType::SEMICOLON);
	case ',': return makeToken(TokenType::COMMA);
	case '.': return makeToken(TokenType::DOT);
	case '-': return makeToken(match('=') ? TokenType::MINUS_EQUAL : match('-') ? TokenType::DECREMENT : TokenType::MINUS);
	case '+': return makeToken(match('=') ? TokenType::PLUS_EQUAL : match('+') ? TokenType::INCREMENT : TokenType::PLUS);
	case '/': return makeToken(match('=') ? TokenType::SLASH_EQUAL : TokenType::SLASH);
	case '*': return makeToken(match('=') ? TokenType::STAR_EQUAL : TokenType::STAR);
	case '&': return makeToken(match('=') ? TokenType::BITWISE_AND_EQUAL : match('&') ? TokenType::AND : TokenType::BITWISE_AND);
	case '|': return makeToken(match('=') ? TokenType::BITWISE_OR_EQUAL : match('|') ? TokenType::OR : TokenType::BITWISE_OR);
	case '^': return makeToken(match('=') ? TokenType::BITWISE_XOR_EQUAL : TokenType::BITWISE_XOR);
	case '%': return makeToken(match('=') ? TokenType::PERCENTAGE_EQUAL : TokenType::PERCENTAGE);
	case '~': return makeToken(TokenType::TILDA);
	case '!':
		return makeToken(match('=') ? TokenType::BANG_EQUAL : TokenType::BANG);
	case '=':						  
		return makeToken(match('=') ? TokenType::EQUAL_EQUAL : TokenType::EQUAL);
	case '<':						  
		return makeToken(match('=') ? TokenType::LESS_EQUAL : match('<') ? TokenType::BITSHIFT_LEFT : TokenType::LESS);
	case '>':						  
		return makeToken(match('=') ? TokenType::GREATER_EQUAL : match('>') ? TokenType::BITSHIFT_RIGHT : TokenType::GREATER);
	case '"': return string_();
	case ':': return makeToken(match(':') ? TokenType::DOUBLE_COLON : TokenType::COLON);
	case '?': return makeToken(TokenType::QUESTIONMARK);
	case '\n':
		curFile->lines.push_back(current);
		line++;
		return makeToken(TokenType::NEWLINE);
	}

	return errorToken("Unexpected character.");
}

Token Scanner::makeToken(TokenType type) {
	Span newSpan(line, start - curFile->lines[curFile->lines.size() - 1], current - start, curFile);
	Token token(newSpan, type);
	string str = token.getLexeme();
	return token;
}

Token Scanner::errorToken(const char* message) {
	return Token(message, line, TokenType::ERROR);
}

char Scanner::peek() {
	if (isAtEnd()) return '\0';
	return curFile->sourceFile[current];
}

char Scanner::peekNext() {
	if (isAtEnd()) return '\0';
	return curFile->sourceFile[current + 1];
}

void Scanner::skipWhitespace() {
	while (true) {
		char c = peek();
		switch (c) {
		case ' ':
		case '\r':
		case '\t':
			advance();
			break;
		case '/':
			if (peekNext() == '/') {
				// A  // comment goes until the end of the line.
				while (peek() != '\n' && !isAtEnd()) advance();
			}
			//if we have a /**/ comment, we loop until we find */
			else if (peekNext() == '*') {
				advance();
				while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
					if (peek() == '\n') {
						line++;
						curFile->lines.push_back(current);
					}
					advance();
				}
				if (!isAtEnd()) {
					advance();
					advance();
				}
			}
			else {
				return;
			}
			break;
		default:
			return;
		}
	}
}

Token Scanner::string_() {
	while (!isAtEnd()) {
		if (peek() == '"') break;
		if (peek() == '\n') {
			line++;
			curFile->lines.push_back(current);
		}
		advance();
	}

	if (isAtEnd()) return errorToken("Unterminated string.");

	// The closing quote.
	advance();
	return makeToken(TokenType::STRING);
}

bool Scanner::isDigit(char c) {
	return c >= '0' && c <= '9';
}

Token Scanner::number() {
	while (isDigit(peek())) advance();

	// Look for a fractional part.
	if (peek() == '.' && isDigit(peekNext())) {
		// Consume the ".".
		advance();

		while (isDigit(peek())) advance();
	}

	return makeToken(TokenType::NUMBER);
}

bool Scanner::isAlpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

//first character of the identifier has to be alphabetical, rest can be alphanumerical + _
Token Scanner::identifier() {
	while (isAlpha(peek()) || isDigit(peek())) advance();
	return makeToken(identifierType());
}

//trie implementation
TokenType Scanner::identifierType() {
	string& source = curFile->sourceFile;
	switch (source[start]) {
	case 'a': return checkKeyword(1, 2, "nd", TokenType::AND);
	case 'b': return checkKeyword(1, 4, "reak", TokenType::BREAK);
	case 'c':
		if (current - start > 1) {
			switch (source[start + 1]) {
			case 'l': return checkKeyword(2, 3, "ass", TokenType::CLASS);
			case 'a': return checkKeyword(2, 2, "se", TokenType::CASE);
			case 'o': return checkKeyword(2, 6, "ntinue", TokenType::CONTINUE);
			}
		}
		break;
	case 'd': return checkKeyword(1, 6, "efault", TokenType::DEFAULT);
	case 'e':
		if (current - start > 1) {
			switch (source[start + 1]) {
			case 'l':  return checkKeyword(2, 2, "se", TokenType::ELSE);
			case 'x':  return checkKeyword(2, 4, "port", TokenType::EXPORT);
			}
		}
	case 'i':
		if (current - start > 1) {
			switch (source[start + 1]) {
			case 'f': return TokenType::IF;
			case 'm': return checkKeyword(2, 4, "port", TokenType::IMPORT);
			}
		}
		break;
	case 'n': return checkKeyword(1, 2, "il", TokenType::NIL);
	case 'm': return checkKeyword(1, 4, "acro", TokenType::MACRO);
	case 'o': return checkKeyword(1, 1, "r", TokenType::OR);
	case 'p': return checkKeyword(1, 4, "rint", TokenType::PRINT);
	case 'r':
		if (current - start > 1) {
			switch (source[start + 1]) {
			case 'e': return checkKeyword(2, 4, "turn", TokenType::RETURN);
			case 'u': return checkKeyword(2, 1, "n", TokenType::RUN);
			}
		}
		break;
	case 's':
		if (current - start > 1) {
			switch (source[start + 1]) {
			case 'u': return checkKeyword(2, 3, "per", TokenType::SUPER);
			case 'w': return checkKeyword(2, 4, "itch", TokenType::SWITCH);
			}
		}
		break;
	case 'v': return checkKeyword(1, 2, "ar", TokenType::VAR);
	case 'w': return checkKeyword(1, 4, "hile", TokenType::WHILE);
	case 'f':
		if (current - start > 1) {
			switch (source[start + 1]) {
			case 'a': return checkKeyword(2, 3, "lse", TokenType::FALSE);
			case 'o': return checkKeyword(2, 1, "r", TokenType::FOR);
			case 'u': return checkKeyword(2, 2, "nc", TokenType::FUNC);
			case 'i': return checkKeyword(2, 3, "ber", TokenType::FIBER);
			}
		}
		break;
	case 't':
		if (current - start > 1) {
			switch (source[start + 1]) {
			case 'h': return checkKeyword(2, 2, "is", TokenType::THIS);
			case 'r': return checkKeyword(2, 2, "ue", TokenType::TRUE);
			}
		}
		break;
	case 'y':
		return checkKeyword(1, 4, "ield", TokenType::YIELD);
	}
	//variable name
	return TokenType::IDENTIFIER;
}

TokenType Scanner::checkKeyword(int strt, int length, const char* rest, TokenType type) {
	if (current - start == strt + length && curFile->sourceFile.substr(start + strt, length).compare(rest) == 0) {
		return type;
	}

	return TokenType::IDENTIFIER;
}
#pragma endregion