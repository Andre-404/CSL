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
		virtual shared_ptr<ASTNode> parse(Token token) = 0;
		Parser* cur;
		int prec;
	};

	class InfixParselet {
	public:
		virtual shared_ptr<ASTNode> parse(shared_ptr<ASTNode> left, Token token, int surroundingPrec) = 0;
		Parser* cur;
		int prec;
	};

	struct TranslationUnit {
		vector<shared_ptr<ASTNode>> stmts;
		CSLModule* src;
		//exported declarations
		vector<Token> exports;

		TranslationUnit(CSLModule* pUnit) {
			src = pUnit;
		}
	};

	class ParserException {

	};

	class Parser {
	public:
		Parser();
		vector<TranslationUnit*> parse(vector<CSLModule*> modules);

	private:
		TranslationUnit* curUnit;
		uInt64 current;

		int loopDepth;
		int switchDepth;

		std::unordered_map<TokenType, unique_ptr<PrefixParselet>> prefixParselets;
		std::unordered_map<TokenType, unique_ptr<InfixParselet>> infixParselets;

		void addPrefix(TokenType type, PrefixParselet* parselet, precedence prec);
		void addInfix(TokenType type, InfixParselet* parselet, precedence prec);

#pragma region Expressions
		shared_ptr<ASTNode> expression(int prec);
		shared_ptr<ASTNode> expression();

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
		shared_ptr<ASTNode> exportDirective();
		shared_ptr<ASTNode> declaration();
		shared_ptr<ASTDecl> varDecl();
		shared_ptr<ASTDecl> funcDecl();
		shared_ptr<ASTDecl> classDecl();

		shared_ptr<ASTNode> statement();
		shared_ptr<ASTNode> printStmt();
		shared_ptr<ASTNode> exprStmt();
		shared_ptr<ASTNode> blockStmt();
		shared_ptr<ASTNode> ifStmt();
		shared_ptr<ASTNode> whileStmt();
		shared_ptr<ASTNode> forStmt();
		shared_ptr<ASTNode> breakStmt();
		shared_ptr<ASTNode> continueStmt();
		shared_ptr<ASTNode> switchStmt();
		shared_ptr<CaseStmt> caseStmt();
		shared_ptr<ASTNode> returnStmt();

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