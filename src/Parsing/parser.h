#pragma once
#include "../common.h"
#include "ASTDefs.h"
#include <initializer_list>
#include <map>
#include <unordered_map>

namespace AST {
	using std::unique_ptr;
	class Parser;

	enum class precedence {
		NONE,
		ASSIGNMENT,
		CONDITIONAL,
		OR,
		AND,
		BIN_OR,
		BIN_XOR,
		BIN_AND,
		EQUALITY,
		COMPARISON,
		BITSHIFT,
		SUM,
		FACTOR,
		NOT,
		ALTER,
		CALL,
		PRIMARY
	};

	class PrefixParselet {
	public:
		virtual ASTNodePtr parse(Token token) = 0;
		Parser* cur;
		int prec;
	};

	class InfixParselet {
	public:
		virtual ASTNodePtr parse(ASTNodePtr left, Token token, int surroundingPrec) = 0;
		Parser* cur;
		int prec;
	};

	class ParserException {

	};

	class Parser {
	public:
		Parser();
		void parse(vector<CSLModule*>& modules);

	private:
		CSLModule* curUnit;
		uInt64 current;

		int loopDepth;
		int switchDepth;

		std::unordered_map<TokenType, unique_ptr<PrefixParselet>> prefixParselets;
		std::unordered_map<TokenType, unique_ptr<InfixParselet>> infixParselets;

		void addPrefix(TokenType type, PrefixParselet* parselet, precedence prec);
		void addInfix(TokenType type, InfixParselet* parselet, precedence prec);

#pragma region Expressions
		ASTNodePtr expression(int prec);
		ASTNodePtr expression();

		//parselets that need to have access to private methods of parser
		friend class fieldAccessExpr;
		friend class callExpr;
		friend class binaryExpr;
		friend class conditionalExpr;
		friend class assignmentExpr;
		friend class literalExpr;
		friend class unaryPrefixExpr;
		friend class unaryPostfixExpr;
#pragma endregion

#pragma region Statements
		ASTNodePtr topLevelDeclaration();
		ASTNodePtr localDeclaration();
		shared_ptr<ASTDecl> varDecl();
		shared_ptr<ASTDecl> funcDecl();
		shared_ptr<ASTDecl> classDecl();

		ASTNodePtr statement();
		ASTNodePtr printStmt();
		ASTNodePtr exprStmt();
		ASTNodePtr blockStmt();
		ASTNodePtr ifStmt();
		ASTNodePtr whileStmt();
		ASTNodePtr forStmt();
		ASTNodePtr breakStmt();
		ASTNodePtr continueStmt();
		ASTNodePtr switchStmt();
		shared_ptr<CaseStmt> caseStmt();
		ASTNodePtr advanceStmt();
		ASTNodePtr returnStmt();

#pragma endregion

#pragma region Helpers

		bool match(const std::initializer_list<TokenType>& tokenTypes);

		bool match(const TokenType type);

		bool isAtEnd();

		bool check(TokenType type);

		Token advance();

		Token peek();

		Token peekNext();

		Token previous();

		Token consume(TokenType type, string msg);

		ParserException error(Token token, string msg);

		void sync();

		int getPrec();
#pragma endregion

	};

}