#include "parser.h"
#include "../ErrorHandling/errorHandler.h"
#include "../DebugPrinting/ASTPrinter.h"

using std::make_shared;
using namespace AST;

//have to define this in the AST namespace because parselets are c++ friend classes
namespace AST {
	//!, -, ~, ++a and --a
	class unaryPrefixExpr : public PrefixParselet {
		ASTNodePtr parse(Token token) {
			ASTNodePtr expr = cur->expression(prec);
			return make_shared<UnaryExpr>(token, expr, true);
		}
	};

	//numbers, string, boolean, nil, array and struct literals, grouping as well as super calls and anonymous functions
	class literalExpr : public PrefixParselet {
		ASTNodePtr parse(Token token) {
			switch (token.type) {
				//only thing that gets inherited is methods
			case TokenType::SUPER: {
				cur->consume(TokenType::DOT, "Expected '.' after super.");
				Token ident = cur->consume(TokenType::IDENTIFIER, "Expect superclass method name.");
				return make_shared<SuperExpr>(ident);
			}
			case TokenType::LEFT_PAREN: {
				//grouping can contain a expr of any precedence
				ASTNodePtr expr = cur->expression();
				cur->consume(TokenType::RIGHT_PAREN, "Expected ')' at the end of grouping expression.");
				return make_shared<GroupingExpr>(expr);
			}
			//Array literal
			case TokenType::LEFT_BRACKET: {
				vector<ASTNodePtr> members;
				if (!(cur->peek().type == TokenType::RIGHT_BRACKET)) {
					do {
						members.push_back(cur->expression());
					} while (cur->match(TokenType::COMMA));
				}
				cur->consume(TokenType::RIGHT_BRACKET, "Expect ']' at the end of an array literal.");
				return make_shared<ArrayLiteralExpr>(members);
			}
			//Struct literal
			case TokenType::LEFT_BRACE: {
				vector<StructEntry> entries;
				if (!(cur->peek().type == TokenType::RIGHT_BRACE)) {
					//a struct literal looks like this: {var1 : expr1, var2 : expr2}
					do {
						Token identifier = cur->consume(TokenType::IDENTIFIER, "Expected a identifier.");
						cur->consume(TokenType::COLON, "Expected a ':' after identifier");
						ASTNodePtr expr = cur->expression();
						entries.emplace_back(identifier, expr);
					} while (cur->match(TokenType::COMMA));
				}
				cur->consume(TokenType::RIGHT_BRACE, "Expect '}' after struct literal.");
				return make_shared<StructLiteral>(entries);
			}
			//function literal
			case TokenType::FUNC: {
				//the depths are used for throwing errors for switch and loops stmts, 
				//and since a function can be declared inside a loop we need to account for that
				int tempLoopDepth = cur->loopDepth;
				int tempSwitchDepth = cur->switchDepth;
				cur->loopDepth = 0;
				cur->switchDepth = 0;

				cur->consume(TokenType::LEFT_PAREN, "Expect '(' for arguments.");
				vector<Token> args;
				//parse args
				if (!cur->check(TokenType::RIGHT_PAREN)) {
					do {
						Token arg = cur->consume(TokenType::IDENTIFIER, "Expect argument name");
						args.push_back(arg);
						if (args.size() > 255) {
							throw cur->error(arg, "Functions can't have more than 255 arguments");
						}
					} while (cur->match(TokenType::COMMA));
				}
				cur->consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments");
				cur->consume(TokenType::LEFT_BRACE, "Expect '{' after arguments.");
				ASTNodePtr body = cur->blockStmt();

				cur->loopDepth = tempLoopDepth;
				cur->switchDepth = tempSwitchDepth;
				return make_shared<FuncLiteral>(args, body);
			}
			case TokenType::AWAIT: {
				ASTNodePtr expr = cur->expression();
				return make_shared<AwaitExpr>(expr);
			}
			//number, string, boolean or nil
			default:
				return make_shared<LiteralExpr>(token);
			}
		}
	};

	//variable assignment
	class assignmentExpr : public InfixParselet {
		ASTNodePtr parse(ASTNodePtr left, Token token, int surroundingPrec) {
			//makes it right associative
			ASTNodePtr right = parseAssign(left, token);
			if (!dynamic_cast<LiteralExpr*>(left.get()) || dynamic_cast<LiteralExpr*>(left.get())->token.type != TokenType::IDENTIFIER) {
				throw cur->error(token, "Left side is not assignable");
			}
			return make_shared<AssignmentExpr>(dynamic_cast<LiteralExpr*>(left.get())->token, right);
		}

		//used for parsing assignment tokens(eg. =, +=, *=...)
		ASTNodePtr parseAssign(ASTNodePtr left, Token op) {
			ASTNodePtr right = cur->expression();
			switch (op.type) {
			case TokenType::EQUAL: {
				break;
			}
			case TokenType::PLUS_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::PLUS, op), right);
				break;
			}
			case TokenType::MINUS_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::MINUS, op), right);
				break;
			}
			case TokenType::SLASH_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::SLASH, op), right);
				break;
			}
			case TokenType::STAR_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::STAR, op), right);
				break;
			}
			case TokenType::BITWISE_XOR_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::BITWISE_XOR, op), right);
				break;
			}
			case TokenType::BITWISE_AND_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::BITWISE_AND, op), right);
				break;
			}
			case TokenType::BITWISE_OR_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::BITWISE_OR, op), right);
				break;
			}
			case TokenType::PERCENTAGE_EQUAL: {
				right = make_shared<BinaryExpr>(left, Token(TokenType::PERCENTAGE, op), right);
				break;
			}
			}
			return right;
		}
	};

	//?: operator
	class conditionalExpr : public InfixParselet {
		ASTNodePtr parse(ASTNodePtr left, Token token, int surroundingPrec) {
			ASTNodePtr thenBranch = cur->expression(prec - 1);
			cur->consume(TokenType::COLON, "Expected ':' after then branch.");
			ASTNodePtr elseBranch = cur->expression(prec - 1);
			return make_shared<ConditionalExpr>(left, thenBranch, elseBranch);
		}
	};

	//any binary operation + module alias access operator(::)
	class binaryExpr : public InfixParselet {
		ASTNodePtr parse(ASTNodePtr left, Token token, int surroundingPrec) {
			if (token.type == TokenType::DOUBLE_COLON) {
				//dynamic cast B2D is dumb but its the best solution for this outlier
				if (dynamic_cast<LiteralExpr*>(left.get())) {
					LiteralExpr* expr = dynamic_cast<LiteralExpr*>(left.get());
					Token ident = cur->consume(TokenType::IDENTIFIER, "Expected variable name.");
					make_shared<ModuleAccessExpr>(expr->token, ident);
				}
				else throw cur->error(token, "Expected module name identifier.");
			}
			ASTNodePtr right = cur->expression(prec);
			return make_shared<BinaryExpr>(left, token, right);
		}
	};

	//a++, a--
	class unaryPostfixExpr : public InfixParselet {
		ASTNodePtr parse(ASTNodePtr var, Token op, int surroundingPrec) {
			return make_shared<UnaryExpr>(op, var, false);
		}
	};

	//function calling
	class callExpr : public InfixParselet {
	public:
		ASTNodePtr parse(ASTNodePtr left, Token token, int surroundingPrec) {
			vector<ASTNodePtr> args;
			if (!cur->check(TokenType::RIGHT_PAREN)) {
				do {
					args.push_back(cur->expression());
				} while (cur->match(TokenType::COMMA));
			}
			cur->consume(TokenType::RIGHT_PAREN, "Expect ')' after call expression.");
			return make_shared<CallExpr>(left, args);
		}
	};

	//accessing struct, class or array fields
	class fieldAccessExpr : public InfixParselet {
	public:
		ASTNodePtr parse(ASTNodePtr left, Token token, int surroundingPrec) {
			ASTNodePtr field = nullptr;
			if (token.type == TokenType::LEFT_BRACKET) {//array/struct with string access
				field = cur->expression();
				cur->consume(TokenType::RIGHT_BRACKET, "Expect ']' after array/map access.");
			}
			else if (token.type == TokenType::DOT) {//struct/object access
				Token fieldName = cur->consume(TokenType::IDENTIFIER, "Expected a field identifier.");
				field = make_shared<LiteralExpr>(fieldName);
			}
			//if we have something like arr[0] = 1 or struct.field = 1 we can't parse it with the assignment expr
			//this handles that case and produces a special set expr
			//we also check the precedence level of the surrounding expression, so "a + b.c = 3" doesn't get parsed
			//the match() covers every possible type of assignment
			if (surroundingPrec <= (int)precedence::ASSIGNMENT
				&& cur->match({ TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, TokenType::SLASH_EQUAL,
					TokenType::STAR_EQUAL, TokenType::BITWISE_XOR_EQUAL, TokenType::BITWISE_AND_EQUAL,
					TokenType::BITWISE_OR_EQUAL, TokenType::PERCENTAGE_EQUAL })) {
				Token op = cur->previous();
				ASTNodePtr val = cur->expression();
				return make_shared<SetExpr>(left, field, token, op, val);
			}
			return make_shared<FieldAccessExpr>(left, token, field);
		}
	};
}

Parser::Parser() {
	current = 0;
	loopDepth = 0;
	switchDepth = 0;
	curUnit = nullptr;

	#pragma region Parselets
	//Prefix
	addPrefix(TokenType::THIS, new literalExpr, precedence::NONE);

	addPrefix(TokenType::BANG, new unaryPrefixExpr, precedence::NOT);
	addPrefix(TokenType::MINUS, new unaryPrefixExpr, precedence::NOT);
	addPrefix(TokenType::TILDA, new unaryPrefixExpr, precedence::NOT);

	addPrefix(TokenType::INCREMENT, new unaryPrefixExpr, precedence::ALTER);
	addPrefix(TokenType::DECREMENT, new unaryPrefixExpr, precedence::ALTER);


	addPrefix(TokenType::IDENTIFIER, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::STRING, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::NUMBER, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::TRUE, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::FALSE, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::NIL, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::LEFT_PAREN, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::LEFT_BRACKET, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::LEFT_BRACE, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::SUPER, new literalExpr, precedence::PRIMARY);
	addPrefix(TokenType::FUNC, new literalExpr, precedence::PRIMARY);

	//Infix
	addInfix(TokenType::EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::PLUS_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::MINUS_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::SLASH_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::STAR_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::PERCENTAGE_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::BITWISE_XOR_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::BITWISE_OR_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);
	addInfix(TokenType::BITWISE_AND_EQUAL, new assignmentExpr, precedence::ASSIGNMENT);

	addInfix(TokenType::QUESTIONMARK, new conditionalExpr, precedence::CONDITIONAL);

	addInfix(TokenType::OR, new binaryExpr, precedence::OR);
	addInfix(TokenType::AND, new binaryExpr, precedence::AND);

	addInfix(TokenType::BITWISE_OR, new binaryExpr, precedence::BIN_OR);
	addInfix(TokenType::BITWISE_XOR, new binaryExpr, precedence::BIN_XOR);
	addInfix(TokenType::BITWISE_AND, new binaryExpr, precedence::BIN_AND);

	addInfix(TokenType::EQUAL_EQUAL, new binaryExpr, precedence::EQUALITY);
	addInfix(TokenType::BANG_EQUAL, new binaryExpr, precedence::EQUALITY);

	addInfix(TokenType::LESS, new binaryExpr, precedence::COMPARISON);
	addInfix(TokenType::LESS_EQUAL, new binaryExpr, precedence::COMPARISON);
	addInfix(TokenType::GREATER, new binaryExpr, precedence::COMPARISON);
	addInfix(TokenType::GREATER_EQUAL, new binaryExpr, precedence::COMPARISON);

	addInfix(TokenType::BITSHIFT_LEFT, new binaryExpr, precedence::BITSHIFT);
	addInfix(TokenType::BITSHIFT_RIGHT, new binaryExpr, precedence::BITSHIFT);

	addInfix(TokenType::PLUS, new binaryExpr, precedence::SUM);
	addInfix(TokenType::MINUS, new binaryExpr, precedence::SUM);

	addInfix(TokenType::SLASH, new binaryExpr, precedence::FACTOR);
	addInfix(TokenType::STAR, new binaryExpr, precedence::FACTOR);
	addInfix(TokenType::PERCENTAGE, new binaryExpr, precedence::FACTOR);

	addInfix(TokenType::LEFT_PAREN, new callExpr, precedence::CALL);
	addInfix(TokenType::LEFT_BRACKET, new fieldAccessExpr, precedence::CALL);
	addInfix(TokenType::DOT, new fieldAccessExpr, precedence::CALL);

	addInfix(TokenType::DOUBLE_COLON, new binaryExpr, precedence::PRIMARY);

	//Postfix
	//postfix and mixfix operators get parsed with the infix parselets
	addInfix(TokenType::INCREMENT, new unaryPostfixExpr, precedence::ALTER);
	addInfix(TokenType::DECREMENT, new unaryPostfixExpr, precedence::ALTER);
#pragma endregion
}

void Parser::parse(vector<CSLModule*> modules) {
	#ifdef AST_DEBUG
	ASTPrinter printer;
	#endif
	//modules are already sorted using topsort
	for (CSLModule* unit : modules) {
		curUnit = unit;

		current = 0;
		loopDepth = 0;
		switchDepth = 0;
		while (!isAtEnd()) {
			try {
				unit->stmts.push_back(topLevelDeclaration());
				#ifdef AST_DEBUG
				//prints statement
				unit->stmts[unit->stmts.size() - 1]->accept(&printer);
				#endif
			}
			catch (ParserException e) {
				sync();
			}
		}

	}
}

ASTNodePtr Parser::expression(int prec) {
	Token token = advance();
	//check if the token has a prefix function associated with it, and if it does, parse with it
	if (prefixParselets.count(token.type) == 0) {
		throw error(token, "Expected expression.");
	}
	unique_ptr<PrefixParselet>& prefix = prefixParselets[token.type];
	shared_ptr<ASTNode> left = prefix->parse(token);

	//advances only if the next token has a higher associativity than the current one
	//eg. 1 + 2 compiles because the base precedence is 0, and '+' has a precedence of 11
	//loop runs as long as the next operator has a higher precedence than the one that called this function
	while (prec < getPrec()) {
		token = advance();
		if (infixParselets.count(token.type) == 0) {
			throw error(token, "Expected expression.");
		}
		unique_ptr<InfixParselet>& infix = infixParselets[token.type];
		left = infix->parse(left, token, prec);
	}
	return left;
}

ASTNodePtr Parser::expression() {
	return expression(0);
}

#pragma region Statements and declarations
//module level variables are put in a list to help with error reporting in compiler
ASTNodePtr Parser::topLevelDeclaration() {
	//export is only allowed in global scope
	if (match(TokenType::EXPORT)) {
		//after export only keywords allowed are: var, class, func
		shared_ptr<ASTDecl> node = nullptr;
		if (match(TokenType::VAR)) node = varDecl();
		else if (match(TokenType::CLASS)) node = classDecl();
		else if (match(TokenType::FUNC)) node = funcDecl();

		curUnit->exports.push_back(node->getName());
		curUnit->topDeclarations.push_back(node->getName());

		return node;

		throw error(peek(), "Expected variable, class or function declaration");
	}
	else {
		shared_ptr<ASTDecl> node = nullptr;
		if (match(TokenType::VAR)) node = varDecl();
		else if (match(TokenType::CLASS)) node = classDecl();
		else if (match(TokenType::FUNC)) node = funcDecl();
		if (node != nullptr) {
			curUnit->topDeclarations.push_back(node->getName());
			return node;
		}
	}
	return declaration();
}

ASTNodePtr Parser::declaration() {
	if (match(TokenType::VAR)) return varDecl();
	else if (match(TokenType::CLASS)) return classDecl();
	else if (match(TokenType::FUNC)) return funcDecl();
	return statement();
}

shared_ptr<ASTDecl> Parser::varDecl() {
	Token name = consume(TokenType::IDENTIFIER, "Expected a variable identifier.");
	ASTNodePtr expr = nullptr;
	//if no initializer is present the variable is initialized to null
	if (match(TokenType::EQUAL)) {
		expr = expression();
	}
	consume(TokenType::SEMICOLON, "Expected a ';' after variable declaration.");
	return make_shared<VarDecl>(name, expr);
}

shared_ptr<ASTDecl> Parser::funcDecl() {
	//the depths are used for throwing errors for switch and loops stmts, 
	//and since a function can be declared inside a loop we need to account for that
	int tempLoopDepth = loopDepth;
	int tempSwitchDepth = switchDepth;
	loopDepth = 0;
	switchDepth = 0;

	Token name = consume(TokenType::IDENTIFIER, "Expected a function name.");
	consume(TokenType::LEFT_PAREN, "Expect '(' after function name.");
	vector<Token> args;
	//parse args
	if (!check(TokenType::RIGHT_PAREN)) {
		do {
			Token arg = consume(TokenType::IDENTIFIER, "Expect argument name");
			args.push_back(arg);
			if (args.size() > 255) {
				throw error(arg, "Functions can't have more than 255 arguments");
			}
		} while (match(TokenType::COMMA));
	}
	consume(TokenType::RIGHT_PAREN, "Expect ')' after arguments");
	consume(TokenType::LEFT_BRACE, "Expect '{' after arguments.");
	ASTNodePtr body = blockStmt();

	loopDepth = tempLoopDepth;
	switchDepth = tempSwitchDepth;
	return make_shared<FuncDecl>(name, args, body);
}

shared_ptr<ASTDecl> Parser::classDecl() {
	Token name = consume(TokenType::IDENTIFIER, "Expected a class name.");
	Token inherited;
	//inheritance is optional
	if (match(TokenType::COLON)) inherited = consume(TokenType::IDENTIFIER, "Expected a parent class name.");
	consume(TokenType::LEFT_BRACE, "Expect '{' before class body.");
	//a class body can contain only methods(fields are initialized in the constructor)
	vector<ASTNodePtr> methods;
	while (!check(TokenType::RIGHT_BRACE) && !isAtEnd()) {
		methods.push_back(funcDecl());
	}
	consume(TokenType::RIGHT_BRACE, "Expect '}' after class body.");
	return make_shared<ClassDecl>(name, methods, inherited, inherited.type == TokenType::IDENTIFIER);
}

ASTNodePtr Parser::statement() {
	if (match({ TokenType::PRINT, TokenType::LEFT_BRACE, TokenType::IF, TokenType::WHILE,
		TokenType::FOR, TokenType::BREAK, TokenType::SWITCH,
		TokenType::RETURN, TokenType::CONTINUE })) {

		switch (previous().type) {
		case TokenType::PRINT: return printStmt();
		case TokenType::LEFT_BRACE: return blockStmt();
		case TokenType::IF: return ifStmt();
		case TokenType::WHILE: return whileStmt();
		case TokenType::FOR: return forStmt();
		case TokenType::BREAK: return breakStmt();
		case TokenType::CONTINUE: return continueStmt();
		case TokenType::SWITCH: return switchStmt();
		case TokenType::RETURN: return returnStmt();
		}
	}
	return exprStmt();
}

//temporary, later we'll remove this and add print as a native function
ASTNodePtr Parser::printStmt() {
	ASTNodePtr expr = expression();
	consume(TokenType::SEMICOLON, "Expected ';' after expression.");
	return make_shared<PrintStmt>(expr);
}

ASTNodePtr Parser::exprStmt() {
	ASTNodePtr expr = expression();
	consume(TokenType::SEMICOLON, "Expected ';' after expression.");
	return make_shared<ExprStmt>(expr);
}

ASTNodePtr Parser::blockStmt() {
	vector<ASTNodePtr> stmts;
	//TokenType::LEFT_BRACE is already consumed
	while (!check(TokenType::RIGHT_BRACE)) {
		stmts.push_back(declaration());
	}
	consume(TokenType::RIGHT_BRACE, "Expect '}' after block.");
	return make_shared<BlockStmt>(stmts);
}

ASTNodePtr Parser::ifStmt() {
	consume(TokenType::LEFT_PAREN, "Expect '(' after 'if'.");
	ASTNodePtr condition = expression();
	consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");
	//using statement() instead of declaration() disallows declarations directly in a control flow body
	//declarations are still allowed in block statement
	ASTNodePtr thenBranch = statement();
	ASTNodePtr elseBranch = nullptr;
	if (match(TokenType::ELSE)) {
		elseBranch = statement();
	}
	return make_shared<IfStmt>(thenBranch, elseBranch, condition);
}

ASTNodePtr Parser::whileStmt() { 
	//loopDepth is used to see if a 'continue' or 'break' statement is allowed within the body
	loopDepth++;
	consume(TokenType::LEFT_PAREN, "Expect '(' after 'while'.");
	ASTNodePtr condition = expression();
	consume(TokenType::RIGHT_PAREN, "Expect ')' after condition.");
	ASTNodePtr body = statement();
	loopDepth--;
	return make_shared<WhileStmt>(body, condition);
}

ASTNodePtr Parser::forStmt() {
	loopDepth++;
	consume(TokenType::LEFT_PAREN, "Expect '(' after 'for'.");
	//initializer can either be: empty, a new variable declaration, or any expression
	ASTNodePtr init = nullptr;
	if (match(TokenType::SEMICOLON)) {
		//do nothing
	}
	else if (match(TokenType::VAR)) init = varDecl();
	else init = exprStmt();

	ASTNodePtr condition = nullptr;
	//we don't want to use exprStmt() because it emits OP_POP, and we'll need the value to determine whether to jump
	if (!check(TokenType::SEMICOLON)) condition = expression();
	consume(TokenType::SEMICOLON, "Expect ';' after loop condition");

	ASTNodePtr increment = nullptr;
	//using expression() here instead of exprStmt() because there is no trailing ';'
	if (!check(TokenType::RIGHT_PAREN)) increment = expression();
	consume(TokenType::RIGHT_PAREN, "Expect ')' after 'for' clauses.");
	//disallows declarations unless they're in a block
	ASTNodePtr body = statement();
	loopDepth--;
	return make_shared<ForStmt>(init, condition, increment, body);
}

ASTNodePtr Parser::breakStmt() {
	if (loopDepth == 0 && switchDepth == 0) throw error(previous(), "Cannot use 'break' outside of loops or switch statements.");
	consume(TokenType::SEMICOLON, "Expect ';' after break.");
	return make_shared<BreakStmt>(previous());
}

ASTNodePtr Parser::continueStmt() {
	if (loopDepth == 0) throw error(previous(), "Cannot use 'continue' outside of loops.");
	consume(TokenType::SEMICOLON, "Expect ';' after continue.");
	return make_shared<ContinueStmt>(previous());
}

ASTNodePtr Parser::switchStmt() {
	//structure:
	//switch(<expression>){
	//case <expression>: <statements>
	//}
	consume(TokenType::LEFT_PAREN, "Expect '(' after 'switch'.");
	ASTNodePtr expr = expression();
	consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
	consume(TokenType::LEFT_BRACE, "Expect '{' after switch expression.");
	switchDepth++;
	vector<shared_ptr<CaseStmt>> cases;
	bool hasDefault = false;

	while (!check(TokenType::RIGHT_BRACE) && match({ TokenType::CASE, TokenType::DEFAULT })) {
		Token prev = previous();//to see if it's a default statement
		shared_ptr<CaseStmt> curCase = caseStmt();
		curCase->caseType = prev;
		if (prev.type == TokenType::DEFAULT) {
			//don't throw, it isn't a breaking error
			if (hasDefault) error(prev, "Only 1 default case is allowed inside a switch statement.");
			hasDefault = true;
		}
		cases.push_back(curCase);
	}
	consume(TokenType::RIGHT_BRACE, "Expect '}' after switch body.");
	switchDepth--;
	return make_shared<SwitchStmt>(expr, cases, hasDefault);
}

shared_ptr<CaseStmt> Parser::caseStmt() {
	Token matchConstant;
	//default cases don't have a match expression
	if (previous().type != TokenType::DEFAULT && match({TokenType::NIL, TokenType::NUMBER, TokenType::STRING, TokenType::TRUE, TokenType::FALSE})) {
		matchConstant = previous();
	}
	else if (previous().type != TokenType::DEFAULT) {
		throw error(previous(), "Expression must be a constant literal(string, number, boolean or nil).");
	}
	consume(TokenType::COLON, "Expect ':' after 'case'.");
	vector<ASTNodePtr> stmts;
	while (!check(TokenType::CASE) && !check(TokenType::RIGHT_BRACE) && !check(TokenType::DEFAULT)) {
		stmts.push_back(statement());
	}
	return make_shared<CaseStmt>(matchConstant, stmts);
}

ASTNodePtr Parser::returnStmt() {
	ASTNodePtr expr = nullptr;
	Token keyword = previous();
	if (!match(TokenType::SEMICOLON)) {
		expr = expression();
		consume(TokenType::SEMICOLON, "Expect ';' at the end of 'return'.");
	}
	return make_shared<ReturnStmt>(expr, keyword);
}

#pragma endregion

#pragma region Helpers
//if the current token type matches any of the provided tokenTypes it's consumed, if not false is returned
bool Parser::match(const std::initializer_list<TokenType>& tokenTypes) {
	for (TokenType type : tokenTypes) {
		if (check(type)) {
			advance();
			return true;
		}
	}
	return false;
}

bool Parser::match(const TokenType type) {
	return match({ type });
}

bool Parser::isAtEnd() {
	return peek().type == TokenType::TOKEN_EOF;
}

bool Parser::check(TokenType type) {
	if (isAtEnd()) return false;
	return peek().type == type;
}

//returns current token and increments to the next one
Token Parser::advance() {
	if (!isAtEnd()) current++;
	return previous();
}

//gets current token
Token Parser::peek() {
	if (curUnit->tokens.size() < current) throw error(curUnit->tokens[current - 1], "Expected token.");
	return curUnit->tokens[current];
}

//gets next token
Token Parser::peekNext() {
	if (curUnit->tokens.size() < current + 1) throw error(curUnit->tokens[current], "Expected token.");
	return curUnit->tokens[current + 1];
}

Token Parser::previous() {
	if (current - 1 < 0) throw error(curUnit->tokens[current], "Expected token.");
	return curUnit->tokens[current - 1];
}

//if the current token is of the correct type, it's consumed, if not an error is thrown
Token Parser::consume(TokenType type, string msg) {
	if (check(type)) return advance();

	throw error(peek(), msg);
}

ParserException Parser::error(Token token, string msg) {
	errorHandler::addCompileError(msg, token);
	return ParserException();
}

//syncs when we find a ';' or one of the keywords
void Parser::sync() {
	advance();

	while (!isAtEnd()) {
		if (previous().type == TokenType::SEMICOLON) return;

		switch (peek().type) {
		case TokenType::CLASS:
		case TokenType::FUNC:
		case TokenType::VAR:
		case TokenType::FOR:
		case TokenType::IF:
		case TokenType::ELSE:
		case TokenType::WHILE:
		case TokenType::PRINT:
		case TokenType::RETURN:
		case TokenType::SWITCH:
		case TokenType::CASE:
		case TokenType::DEFAULT:
		case TokenType::RIGHT_BRACE:
			return;
		}

		advance();
	}
}

void Parser::addPrefix(TokenType type, PrefixParselet* parselet, precedence prec) {
	parselet->cur = this;
	parselet->prec = (int)prec;
	prefixParselets.insert_or_assign(type, unique_ptr<PrefixParselet>(parselet));
}

void Parser::addInfix(TokenType type, InfixParselet* parselet, precedence prec) {
	parselet->cur = this;
	parselet->prec = (int)prec;
	infixParselets.insert_or_assign(type, unique_ptr<InfixParselet>(parselet));
}

//checks if the current token has any infix parselet associated with it, and if so the precedence of that operation is returned
int Parser::getPrec() {
	Token token = peek();
	if (infixParselets.count(token.type) == 0) {
		return 0;
	}
	return infixParselets[token.type]->prec;
}
#pragma endregion
