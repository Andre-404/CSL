#include "compiler.h"
#include "../MemoryManagment/garbageCollector.h"
#include "../ErrorHandling/errorHandler.h"

using namespace compileCore;
using namespace object;

CurrentChunkInfo::CurrentChunkInfo(CurrentChunkInfo* _enclosing, funcType _type) : enclosing(_enclosing), type(_type) {
	//first slot is claimed for function name
	upvalues = std::array<Upvalue, UPVAL_MAX>();
	hasReturnStmt = false;
	hasCapturedLocals = false;
	localCount = 0;
	scopeDepth = 0;
	line = 0;
	Local* local = &locals[localCount++];
	local->depth = 0;
	if (type != funcType::TYPE_FUNC) {
		local->name = "this";
	}
	else {
		local->name = "";
	}
	func = new ObjFunc();
}


Compiler::Compiler(vector<AST::TranslationUnit*>& units) {
	current = new CurrentChunkInfo(nullptr, funcType::TYPE_SCRIPT);
	currentClass = nullptr;
	vector<File*> sourceFiles;

	for (AST::TranslationUnit* unit : units) {
		curUnit = unit;
		sourceFiles.push_back(unit->src->file);
		for (int i = 0; i < unit->stmts.size(); i++) {
			//doing this here so that even if a error is detected, we go on and possibly catch other(valid) errors
			try {
				unit->stmts[i]->accept(this);
			}
			catch (CompilerException e) {
				
			}
		}
	}
	for (AST::TranslationUnit* unit : units) delete unit;
}


void Compiler::visitAssignmentExpr(AST::AssignmentExpr* expr) {
	expr->value->accept(this);//compile the right side of the expression
	namedVar(expr->name, true);
}

void Compiler::visitSetExpr(AST::SetExpr* expr) {
	//different behaviour for '[' and '.'
	updateLine(expr->accessor);
	switch (expr->accessor.type) {
	case TokenType::LEFT_BRACKET: {
		expr->callee->accept(this);
		expr->field->accept(this);
		expr->value->accept(this);
		emitByte(+OpCode::SET);
		break;
	}
	case TokenType::DOT: {
		//the "." is always followed by a field name as a string, emitting a constant speeds things up and avoids unnecessary stack manipulation
		expr->callee->accept(this);
		expr->value->accept(this);
		uInt16 name = identifierConstant(dynamic_cast<AST::LiteralExpr*>(expr->field.get())->token);
		if (name < UINT8_MAX) emitBytes(+OpCode::SET_PROPERTY, name);
		else emitByteAnd16Bit(+OpCode::SET_PROPERTY_LONG, name);
		break;
	}
	}
}

void Compiler::visitConditionalExpr(AST::ConditionalExpr* expr) {
	//compile condition and emit a jump over then branch if the condition is false
	expr->condition->accept(this);
	int thenJump = emitJump(+OpCode::JUMP_IF_FALSE_POP);
	expr->thenBranch->accept(this);
	//prevents fallthrough to else branch
	int elseJump = emitJump(+OpCode::JUMP);
	patchJump(thenJump);
	//no need to check if a else branch exists since it's required by the conditional statement
	expr->elseBranch->accept(this);
	patchJump(elseJump);
}

void Compiler::visitBinaryExpr(AST::BinaryExpr* expr) {
	updateLine(expr->op);
	expr->left->accept(this);
	if (expr->op.type == TokenType::OR) {
		//if the left side is true, we know that the whole expression will eval to true
		int jump = emitJump(+OpCode::JUMP_IF_TRUE);
		//pop the left side and eval the right side, right side result becomes the result of the whole expression
		emitByte(+OpCode::POP);
		expr->right->accept(this);
		patchJump(jump);//left side of the expression becomes the result of the whole expression
		return;
	}
	else if (expr->op.type == TokenType::AND) {
		//at this point we have the left side of the expression on the stack, and if it's false we skip to the end
		//since we know the whole expression will evaluate to false
		int jump = emitJump(+OpCode::JUMP_IF_FALSE);
		//if the left side is true, we pop it and then push the right side to the stack, and the result of right side becomes the result of
		//whole expression
		emitByte(+OpCode::POP);
		expr->right->accept(this);
		patchJump(jump);
		return;
	}
	uint8_t op = 0;
	switch (expr->op.type) {
		//take in double or string(in case of add)
	case TokenType::PLUS:	op = +OpCode::ADD; break;
	case TokenType::MINUS:	op = +OpCode::SUBTRACT; break;
	case TokenType::SLASH:	op = +OpCode::DIVIDE; break;
	case TokenType::STAR:	op = +OpCode::MULTIPLY; break;
		//operands get cast to int for these ops
	case TokenType::PERCENTAGE:		op = +OpCode::MOD; break;
	case TokenType::BITSHIFT_LEFT:	op = +OpCode::BITSHIFT_LEFT; break;
	case TokenType::BITSHIFT_RIGHT:	op = +OpCode::BITSHIFT_RIGHT; break;
	case TokenType::BITWISE_AND:	op = +OpCode::BITWISE_AND; break;
	case TokenType::BITWISE_OR:		op = +OpCode::BITWISE_OR; break;
	case TokenType::BITWISE_XOR:	op = +OpCode::BITWISE_XOR; break;
		//these return bools
	case TokenType::EQUAL_EQUAL:	 op = +OpCode::EQUAL; break;
	case TokenType::BANG_EQUAL:		 op = +OpCode::NOT_EQUAL; break;
	case TokenType::GREATER:		 op = +OpCode::GREATER; break;
	case TokenType::GREATER_EQUAL:	 op = +OpCode::GREATER_EQUAL; break;
	case TokenType::LESS:			 op = +OpCode::LESS; break;
	case TokenType::LESS_EQUAL:		 op = +OpCode::LESS_EQUAL; break;
	}
	expr->right->accept(this);
	emitByte(op);
}

void Compiler::visitUnaryExpr(AST::UnaryExpr* expr) {
	expr->right->accept(this);
	updateLine(expr->op);
	if (!expr->isPrefix) {

	}
	else {
		switch (expr->op.type) {
		case TokenType::MINUS: emitByte(+OpCode::NEGATE); break;
		case TokenType::BANG: emitByte(+OpCode::NOT); break;
		case TokenType::TILDA: emitByte(+OpCode::BIN_NOT); break;
		}
	}
}

void Compiler::visitArrayDeclExpr(AST::ArrayLiteralExpr* expr) {
	//we need all of the array member values to be on the stack prior to executing "OP_CREATE_ARRAY"
	for (int i = expr->members.size() - 1; i >= 0; --i) {
		expr->members[i]->accept(this);
	}
	emitBytes(+OpCode::CREATE_ARRAY, expr->members.size());
}

void Compiler::visitCallExpr(AST::CallExpr* expr) {
	//invoking is field access + call, when the compiler recognizes this pattern it optimizes
	if (invoke(expr)) return;
	expr->callee->accept(this);
	int argCount = 0;
	for (AST::ASTNodePtr arg : expr->args) {
		arg->accept(this);
		argCount++;
	}
	emitBytes(+OpCode::CALL, argCount);
	
}

void Compiler::visitFieldAccessExpr(AST::FieldAccessExpr* expr) {
	updateLine(expr->accessor);
	expr->callee->accept(this);
	switch (expr->accessor.type) {
	//these are things like array[index] or object["propertyAsString"]
	case TokenType::LEFT_BRACKET: {
		expr->field->accept(this);
		emitByte(+OpCode::GET);
		break;
	}
	case TokenType::DOT:
		uInt16 name = identifierConstant(dynamic_cast<AST::LiteralExpr*>(expr->field.get())->token);
		if (name < UINT8_MAX) emitBytes(+OpCode::GET_PROPERTY, name);
		else emitByteAnd16Bit(+OpCode::GET_PROPERTY_LONG, name);
		break;
	}
}

void Compiler::visitGroupingExpr(AST::GroupingExpr* expr) {
	expr->expr->accept(this);
}

void Compiler::visitStructLiteralExpr(AST::StructLiteral* expr) {
	vector<int> constants;

	//for each field, compile it and get the constant of the field name
	for (AST::StructEntry entry : expr->fields) {
		entry.expr->accept(this);
		updateLine(entry.name);
		constants.push_back(identifierConstant(entry.name));
	}
	//since the amount of fields is variable, we emit the number of fields follwed by constants for each field
	//it's important that the constants are emitted in reverse order, this is because when the VM gets to this point
	//all of the values we need will be on the stack, and getting them from the back(by popping) will give us the values in reverse order
	if (constants[constants.size() - 1] < UINT8_MAX) {
		emitBytes(+OpCode::CREATE_STRUCT, constants.size());

		for (int i = constants.size() - 1; i >= 0; i--) emitByte(constants[i]);
	}
	else {
		emitByteAnd16Bit(+OpCode::CREATE_STRUCT_LONG, constants.size());

		for (int i = constants.size() - 1; i >= 0; i--) emit16Bit(constants[i]);
	}
}

void Compiler::visitSuperExpr(AST::SuperExpr* expr) {
	int name = identifierConstant(expr->methodName);
	if (currentClass == nullptr) {
		error(expr->methodName, "Can't use 'super' outside of a class.");
	}
	else if (!currentClass->hasSuperclass) {
		error(expr->methodName, "Can't use 'super' in a class with no superclass.");
	}
	//we use syntethic tokens since we know that 'super' and 'this' are defined if we're currently compiling a class method
	namedVar(syntheticToken("this"), false);
	namedVar(syntheticToken("super"), false);
	if (name < UINT8_MAX) emitBytes(+OpCode::GET_SUPER, name);
	else emitByteAnd16Bit(+OpCode::GET_SUPER_LONG, name);
}

void Compiler::visitLiteralExpr(AST::LiteralExpr* expr) {
	updateLine(expr->token);
	switch (expr->token.type) {
	case TokenType::NUMBER: {
		double num = std::stod(expr->token.getLexeme());//doing this becuase stod doesn't accept string_view
		emitConstant(Value(num));
		break;
	}
	case TokenType::TRUE: emitByte(+OpCode::TRUE); break;
	case TokenType::FALSE: emitByte(+OpCode::FALSE); break;
	case TokenType::NIL: emitByte(+OpCode::NIL); break;
	case TokenType::STRING: {
		//this gets rid of quotes, "Hello world"->Hello world
		string temp = expr->token.getLexeme();
		temp.erase(0, 1);
		temp.erase(temp.size() - 1, 1);
		emitConstant(Value(ObjString::createString((char*)temp.c_str(), temp.length(), internedStrings)));
		break;
	}

	case TokenType::THIS: {
		if (currentClass == nullptr) {
			error(expr->token, "Can't use 'this' outside of a class.");
			break;
		}
		//'this' get implicitly defined by the compiler
		namedVar(syntheticToken("this"), false);
		break;
	}
	case TokenType::IDENTIFIER: {
		namedVar(expr->token, false);
		break;
	}
	}
}

void Compiler::visitFuncLiteral(AST::FuncLiteral* expr) {

}
void Compiler::visitModuleAccessExpr(AST::ModuleAccessExpr* expr) {

}



void Compiler::visitVarDecl(AST::VarDecl* decl) {
	//if this is a global, we get a string constant index, if it's a local, it returns a dummy 0;
	uInt16 global = parseVar(decl->name);
	//compile the right side of the declaration, if there is no right side, the variable is initialized as nil
	AST::ASTNodePtr expr = decl->value;
	if (expr == NULL) {
		emitByte(+OpCode::NIL);
	}
	else {
		expr->accept(this);
	}
	//if this is a global var, we emit the code to define it in the VMs global hash table, if it's a local, do nothing
	//the slot that the compiled value is at becomes a local var
	defineVar(global);
}

void Compiler::visitFuncDecl(AST::FuncDecl* decl) {
	uInt16 name = parseVar(decl->getName());
	markInit();
	//creating a new compilerInfo sets us up with a clean slate for writing bytecode, the enclosing functions info
	//is stored in current->enclosing
	current = new CurrentChunkInfo(current, funcType::TYPE_FUNC);
	//current->func = new objFunc();
	//no need for a endScope, since returning from the function discards the entire callstack
	beginScope();
	//we define the args as locals, when the function is called, the args will be sitting on the stack in order
	//we just assign those positions to each arg
	for (Token& var : decl->args) {
		updateLine(var);
		uint8_t constant = parseVar(var);
		defineVar(constant);
	}
	decl->getBody()->accept(this);
	current->func->arity = decl->getArgs().size();
	//could get away with using string instead of objString?
	string str = decl->getName().getLexeme();
	current->func->name = copyString((char*)str.c_str(), str.length());
	//have to do this here since endFuncDecl() deletes the compilerInfo
	std::array<upvalue, UPVAL_MAX> upvals = current->upvalues;

	objFunc* func = endFuncDecl();
	if (func->upvalueCount == 0) {
		gc.cachePtr(func);
		objClosure* closure = new objClosure(dynamic_cast<objFunc*>(gc.getCachedPtr()));
		emitConstant(OBJ_VAL(closure));
		defineVar(name);
		return;
	}

	uInt16 constant = makeConstant(OBJ_VAL(func));

	if (constant < UINT8_MAX) emitBytes(OP_CLOSURE, constant);
	else emitByteAnd16Bit(OP_CLOSURE_LONG, constant);
	//if this function does capture any upvalues, we emit the code for getting them, 
	//when we execute "OP_CLOSURE" we will check to see how many upvalues the function captures by going directly to the func->upvalueCount
	for (int i = 0; i < func->upvalueCount; i++) {
		emitByte(upvals[i].isLocal ? 1 : 0);
		emitByte(upvals[i].index);
	}
	defineVar(name);
}

void compiler::visitClassDecl(ASTClass* decl) {
	Token className = decl->getName();
	uInt16 constant = identifierConstant(className);
	declareVar(className);

	emitByteAnd16Bit(OP_CLASS, constant);

	//define the class here, so that we can use it inside it's own methods
	defineVar(constant);

	classCompilerInfo temp(currentClass, false);
	currentClass = &temp;

	if (decl->inherits()) {
		//if the class does inherit from some other class, we load the parent class and declare 'super' as a local variable
		//which holds said class
		namedVar(decl->getInherited(), false);
		if (identifiersEqual(className.getLexeme(), decl->getInherited().getLexeme())) {
			error(decl->getInherited(), "A class can't inherit from itself.");
		}
		beginScope();
		addLocal(syntheticToken("super"));
		defineVar(0);

		namedVar(className, false);
		emitByte(OP_INHERIT);
		currentClass->hasSuperclass = true;
	}
	//we need to load the class onto the top of the stack so that 'this' keyword can work correctly inside of methods
	//the class that 'this' refers to is captured as a upvalue inside of methods
	if (!decl->inherits()) namedVar(className, false);
	for (ASTNode* _method : decl->getMethods()) {
		method((ASTFunc*)_method, className);
	}
	//popping the current class
	emitByte(OP_POP);

	if (currentClass->hasSuperclass) {
		endScope();
	}
	currentClass = currentClass->enclosing;
}



void compiler::visitPrintStmt(ASTPrintStmt* stmt) {
	stmt->getExpr()->accept(this);
	//OP_TO_STRING is emitted first to handle the case of a instance whose class defines a toString method
	emitBytes(OP_TO_STRING, OP_PRINT);
}

void compiler::visitExprStmt(ASTExprStmt* stmt) {
	stmt->getExpr()->accept(this);
	emitByte(OP_POP);
}

void compiler::visitBlockStmt(ASTBlockStmt* stmt) {
	beginScope();
	vector<ASTNode*> stmts = stmt->getStmts();
	for (ASTNode* node : stmts) {
		node->accept(this);
	}
	endScope();
}

void compiler::visitIfStmt(ASTIfStmt* stmt) {
	//compile condition and emit a jump over then branch if the condition is false
	stmt->getCondition()->accept(this);
	int thenJump = emitJump(OP_JUMP_IF_FALSE_POP);
	stmt->getThen()->accept(this);
	//only compile if there is a else branch
	if (stmt->getElse() != NULL) {
		//prevents fallthrough to else branch
		int elseJump = emitJump(OP_JUMP);
		patchJump(thenJump);

		stmt->getElse()->accept(this);
		patchJump(elseJump);
	}
	else patchJump(thenJump);

}

void compiler::visitWhileStmt(ASTWhileStmt* stmt) {
	//the bytecode for this is almost the same as if statement
	//but at the end of the body, we loop back to the start of the condition
	int loopStart = getChunk()->code.count();
	stmt->getCondition()->accept(this);
	int jump = emitJump(OP_JUMP_IF_FALSE_POP);
	stmt->getBody()->accept(this);
	patchContinue();
	emitLoop(loopStart);
	patchJump(jump);
	patchBreak();
}

void compiler::visitForStmt(ASTForStmt* stmt) {
	//we wrap this in a scope so if there is a var declaration in the initialization it's scoped to the loop
	beginScope();
	if (stmt->getInit() != NULL) stmt->getInit()->accept(this);
	int loopStart = getChunk()->code.count();
	//only emit the exit jump code if there is a condition expression
	int exitJump = -1;
	if (stmt->getCondition() != NULL) {
		stmt->getCondition()->accept(this);
		exitJump = emitJump(OP_JUMP_IF_FALSE_POP);
	}
	//body is mandatory
	stmt->getBody()->accept(this);
	//patching continue here to increment if a variable for incrementing has been defined
	patchContinue();
	//if there is a increment expression, we compile it and emit a POP to get rid of the result
	if (stmt->getIncrement() != NULL) {
		stmt->getIncrement()->accept(this);
		emitByte(OP_POP);
	}
	emitLoop(loopStart);
	//exit jump still needs to handle scoping appropriately 
	if (exitJump != -1) patchJump(exitJump);
	endScope();
	//patch breaks AFTER we close the for scope because breaks that are in current scope aren't patched
	patchBreak();
}

void compiler::visitForeachStmt(ASTForeachStmt* stmt) {
	beginScope();
	//the name of the variable is starts with "0" because no user defined variable can start with a number,
	//and we don't want the underlying code to clash with some user defined variables
	Token iterator = syntheticToken("0iterable");
	//get the iterator
	stmt->getCollection()->accept(this);
	uInt16 name = identifierConstant(syntheticToken("begin"));
	emitByte(OP_INVOKE);
	emitBytes(name, 0);
	addLocal(iterator);
	defineVar(0);
	//Get the var ready
	emitByte(OP_NIL);
	addLocal(stmt->getVarName());
	defineVar(0);

	int loopStart = getChunk()->code.count();

	//advancing
	namedVar(iterator, false);
	name = identifierConstant(syntheticToken("next"));
	emitByte(OP_INVOKE);
	emitBytes(name, 0);
	int jump = emitJump(OP_JUMP_IF_FALSE_POP);

	//get new variable
	namedVar(iterator, false);
	name = identifierConstant(syntheticToken("current"));
	emitBytes(OP_GET_PROPERTY, name);
	namedVar(stmt->getVarName(), true);
	emitByte(OP_POP);

	stmt->getBody()->accept(this);

	//end of loop
	patchContinue();
	emitLoop(loopStart);
	patchJump(jump);
	endScope();
	patchBreak();
}

void compiler::visitBreakStmt(ASTBreakStmt* stmt) {
	//the amount of variables to pop and the amount of code to jump is determined in patchBreak()
	//which is called at the end of loops
	updateLine(stmt->getToken());
	emitByte(OP_JUMP_POPN);
	int breakJump = getChunk()->code.count();
	emitBytes(0xff, 0xff);
	emitBytes(0xff, 0xff);
	current->breakStmts.emplace_back(current->scopeDepth, breakJump, current->localCount);
}

void compiler::visitContinueStmt(ASTContinueStmt* stmt) {
	//the amount of variables to pop and the amount of code to jump is determined in patchContinue()
	//which is called at the end of loops
	updateLine(stmt->getToken());
	emitByte(OP_JUMP_POPN);
	int continueJump = getChunk()->code.count();
	emitBytes(0xff, 0xff);
	emitBytes(0xff, 0xff);
	current->continueStmts.emplace_back(current->scopeDepth, continueJump, current->localCount);
}

void compiler::visitSwitchStmt(ASTSwitchStmt* stmt) {
	beginScope();
	//compile the expression in ()
	stmt->getExpr()->accept(this);
	//based on the switch stmt type(all num, all string or mixed) we create a new switch table struct and get it's pos
	//passing in the size to call .reserve() on map/vector
	switchType type = stmt->getType();
	int pos = getChunk()->addSwitch(switchTable(type, stmt->getCases().size()));
	switchTable& table = getChunk()->switchTables[pos];
	emitBytes(OP_SWITCH, pos);

	long start = getChunk()->code.count();
	for (ASTNode* _case : stmt->getCases()) {
		ASTCase* curCase = (ASTCase*)_case;
		//based on the type of switch stmt, we either convert all token lexemes to numbers,
		//or add the strings to a hash table
		if (!curCase->getDef()) {
			switch (type) {
			case switchType::NUM: {
				ASTLiteralExpr* expr = (ASTLiteralExpr*)curCase->getExpr();
				updateLine(expr->getToken());
				int key = std::stoi(string(expr->getToken().getLexeme()));
				long _ip = getChunk()->code.count() - start;

				table.addToArr(key, _ip);
				break;
			}
								//for both strings and mixed switches we pass them as strings
			case switchType::STRING:
			case switchType::MIXED: {
				ASTLiteralExpr* expr = (ASTLiteralExpr*)curCase->getExpr();
				updateLine(expr->getToken());
				long _ip = getChunk()->code.count() - start;
				//converts string_view to string and get's rid of ""
				string _temp(expr->getToken().getLexeme());
				if (expr->getToken().type == TOKEN_STRING) {
					_temp.erase(0, 1);
					_temp.erase(_temp.size() - 1, 1);
				}
				table.addToTable(_temp, _ip);
				break;
			}
			}
		}
		else {
			table.defaultJump = getChunk()->code.count() - start;
		}
		curCase->accept(this);
	}
	//implicit default if the user hasn't defined one, jumps to the end of switch stmt
	if (table.defaultJump == -1) table.defaultJump = getChunk()->code.count() - start;
	//we use a scope and patch breaks AFTER ending the scope because breaks that are in the current scope aren't patched
	endScope();
	patchBreak();
}

void compiler::visitCase(ASTCase* stmt) {
	//compile every statement in the case
	//user has to worry about fallthrough
	for (ASTNode* stmt : stmt->getStmts()) {
		stmt->accept(this);
	}
}

void compiler::visitReturnStmt(ASTReturn* stmt) {
	updateLine(stmt->getKeyword());
	if (current->type == funcType::TYPE_SCRIPT) {
		error(stmt->getKeyword(), "Can't return from top-level code.");
	}
	else if (current->type == funcType::TYPE_CONSTRUCTOR) {
		error(stmt->getKeyword(), "Can't return a value from a constructor.");
	}
	else if (current->type == funcType::TYPE_FIBER) {
		error(stmt->getKeyword(), "Use 'yield' to return value from a fiber and pause it's execution.");
	}
	if (stmt->getExpr() == NULL) {
		emitReturn();
		return;
	}
	stmt->getExpr()->accept(this);
	emitByte(OP_RETURN);
	current->hasReturn = true;
}

#pragma region helpers

#pragma region Emitting bytes

void compiler::emitByte(uint8_t byte) {
	getChunk()->writeData(byte, current->line, curUnit->name);//line is incremented whenever we find a statement/expression that contains tokens
}

void compiler::emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

void compiler::emit16Bit(uInt16 number) {
	//Little endian
	emitBytes((number >> 8) & 0xff, number & 0xff);
}

void compiler::emitByteAnd16Bit(uint8_t byte, uInt16 num) {
	emitByte(byte);
	emit16Bit(num);
}

uInt16 compiler::makeConstant(Value value) {
	if (IS_OBJ(value)) gc.cachePtr(AS_OBJ(value));
	uInt16 constant = getChunk()->addConstant(value);
	if (constant > UINT16_MAX) {
		error("Too many constants in one chunk.");
		return 0;
	}
	//since the pointer in value is now a cached ptr, it doesn't change if a resize/collection occurs, so we need to take care of that
	if (IS_OBJ(value)) AS_OBJ(getChunk()->constants[constant]) = dynamic_cast<obj*>(gc.getCachedPtr());
	return constant;
}

void compiler::emitConstant(Value value) {
	//shorthand for adding a constant to the chunk and emitting it
	uInt16 constant = makeConstant(value);
	if (constant < 256) emitBytes(OP_CONSTANT, constant);
	else if (constant < 1 << 15) emitByteAnd16Bit(OP_CONSTANT_LONG, constant);
}

void compiler::emitGlobalVar(Token name, bool canAssign) {
	uInt arg = identifierConstant(name);
	uInt mod = makeConstant(OBJ_VAL(modules[modules.size() - 1]));
	uint8_t getOp = OP_GET_GLOBAL;
	uint8_t setOp = OP_SET_GLOBAL;
	if (arg > UINT8_MAX || mod > UINT8_MAX) {
		getOp = OP_GET_GLOBAL_LONG;
		setOp = OP_SET_GLOBAL_LONG;
		emitByteAnd16Bit(canAssign ? setOp : getOp, arg);
		emit16Bit(mod);
		return;
	}
	emitByte(canAssign ? setOp : getOp);
	emitBytes(arg, mod);
}


void compiler::emitReturn() {
	if (current->type == funcType::TYPE_CONSTRUCTOR) emitBytes(OP_GET_LOCAL, 0);
	else emitByte(OP_NIL);
	emitByte(OP_RETURN);
}

int compiler::emitJump(uint8_t jumpType) {
	emitByte(jumpType);
	emitBytes(0xff, 0xff);
	return getChunk()->code.count() - 2;
}

void compiler::patchJump(int offset) {
	// -2 to adjust for the bytecode for the jump offset itself.
	int jump = getChunk()->code.count() - offset - 2;
	//fix for future: insert 2 more bytes into the array, but make sure to do the same in lines array
	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}
	getChunk()->code[offset] = (jump >> 8) & 0xff;
	getChunk()->code[offset + 1] = jump & 0xff;
}

void compiler::emitLoop(int start) {
	emitByte(OP_LOOP);

	int offset = getChunk()->code.count() - start + 2;
	if (offset > UINT16_MAX) error("Loop body too large.");

	emit16Bit(offset);
}

#pragma endregion

#pragma region Variables

bool identifiersEqual(const string& a, const string& b) {
	return (a.compare(b) == 0);
}

uInt16 compiler::identifierConstant(Token name) {
	updateLine(name);
	string temp = name.getLexeme();
	//since str is a cached pointer(a collection may occur while resizing the chunk constants array, we need to treat it as a cached ptr
	uInt16 index = makeConstant(OBJ_VAL(copyString((char*)temp.c_str(), temp.length())));

	return index;
}

void compiler::defineVar(uInt16 name) {
	//if this is a local var, mark it as ready and then bail out
	if (current->scopeDepth > 0) {
		markInit();
		return;
	}
	if (name < UINT8_MAX) emitBytes(OP_DEFINE_GLOBAL, name);
	else {
		emitByteAnd16Bit(OP_DEFINE_GLOBAL_LONG, name);
	}
}

void compiler::namedVar(Token token, bool canAssign) {
	updateLine(token);
	uint8_t getOp;
	uint8_t setOp;
	uInt arg = resolveLocal(token);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else if ((arg = resolveUpvalue(current, token)) != -1) {
		getOp = OP_GET_UPVALUE;
		setOp = OP_SET_UPVALUE;
	}
	else {
		return emitGlobalVar(token, canAssign);
	}
	emitBytes(canAssign ? setOp : getOp, arg);
}

uInt16 compiler::parseVar(Token name) {
	updateLine(name);
	declareVar(name);
	if (current->scopeDepth > 0) return 0;
	return identifierConstant(name);
}

void compiler::declareVar(Token& name) {
	updateLine(name);
	if (current->scopeDepth == 0) return;
	for (int i = current->localCount - 1; i >= 0; i--) {
		local* _local = &current->locals[i];
		if (_local->depth != -1 && _local->depth < current->scopeDepth) {
			break;
		}
		string str = name.getLexeme();
		if (identifiersEqual(str, _local->name)) {
			error(name, "Already a variable with this name in this scope.");
		}
	}
	addLocal(name);
}

void compiler::addLocal(Token name) {
	updateLine(name);
	if (current->localCount == LOCAL_MAX) {
		error(name, "Too many local variables in function.");
		return;
	}
	local* _local = &current->locals[current->localCount++];
	_local->name = name.getLexeme();
	_local->depth = -1;
}

void compiler::endScope() {
	//Pop every variable that was declared in this scope
	current->scopeDepth--;//first lower the scope, the check for every var that is deeper than the current scope
	int toPop = 0;
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		if (!current->hasCapturedLocals) toPop++;
		else {
			if (current->locals[current->localCount - 1].isCaptured) {
				emitByte(OP_CLOSE_UPVALUE);
			}
			else {
				emitByte(OP_POP);
			}
		}
		current->localCount--;
	}
	if (toPop > 0 && !current->hasCapturedLocals) emitBytes(OP_POPN, toPop);
}

int compiler::resolveLocal(compilerInfo* func, Token& name) {
	//checks to see if there is a local variable with a provided name, if there is return the index of the stack slot of the var
	updateLine(name);
	for (int i = func->localCount - 1; i >= 0; i--) {
		local* _local = &func->locals[i];
		string str = name.getLexeme();
		if (identifiersEqual(str, _local->name)) {
			if (_local->depth == -1) {
				error(name, "Can't read local variable in its own initializer.");
			}
			return i;
		}
	}

	return -1;
}

int compiler::resolveLocal(Token& name) {
	return resolveLocal(current, name);
}

int compiler::resolveUpvalue(compilerInfo* func, Token& name) {
	if (func->enclosing == NULL) return -1;

	int local = resolveLocal(func->enclosing, name);
	if (local != -1) {
		func->enclosing->locals[local].isCaptured = true;
		func->enclosing->hasCapturedLocals = true;
		return addUpvalue((uint8_t)local, true);
	}
	if (func->type == funcType::TYPE_FIBER) return -1;
	int upvalue = resolveUpvalue(func->enclosing, name);
	if (upvalue != -1) {
		return addUpvalue((uint8_t)upvalue, false);
	}

	return -1;
}

int compiler::addUpvalue(uint8_t index, bool isLocal) {
	int upvalueCount = current->func->upvalueCount;
	for (int i = 0; i < upvalueCount; i++) {
		upvalue* upval = &current->upvalues[i];
		if (upval->index == index && upval->isLocal == isLocal) {
			return i;
		}
	}
	if (upvalueCount == UPVAL_MAX) {
		error("Too many closure variables in function.");
		return 0;
	}
	current->upvalues[upvalueCount].isLocal = isLocal;
	current->upvalues[upvalueCount].index = index;
	return current->func->upvalueCount++;
}

void compiler::markInit() {
	if (current->scopeDepth == 0) return;
	//marks variable as ready to use, any use of it before this call is a error
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

void compiler::patchBreak() {
	int curCode = getChunk()->code.count();
	//most recent breaks are going to be on top
	for (int i = current->breakStmts.size() - 1; i >= 0; i--) {
		jump curBreak = current->breakStmts[i];
		//if the break stmt we're looking at has a depth that's equal or higher than current depth, we bail out
		if (curBreak.depth > current->scopeDepth) {
			int jumpLenght = curCode - curBreak.offset - 4;
			int toPop = curBreak.varNum - current->localCount;
			//variables declared by the time we hit the break whose depth is lower or equal to this break stmt
			getChunk()->code[curBreak.offset] = (toPop >> 8) & 0xff;
			getChunk()->code[curBreak.offset + 1] = toPop & 0xff;
			//amount to jump
			getChunk()->code[curBreak.offset + 2] = (jumpLenght >> 8) & 0xff;
			getChunk()->code[curBreak.offset + 3] = jumpLenght & 0xff;
			//delete break from array
			current->breakStmts.pop_back();
		}
		else break;
	}
}

void compiler::patchContinue() {
	int curCode = getChunk()->code.count();
	//most recent continues are going to be on top
	for (int i = current->continueStmts.size() - 1; i >= 0; i--) {
		jump curContinue = current->continueStmts[i];
		//if the break stmt we're looking at has a depth that's equal or higher than current depth, we bail out
		if (curContinue.depth >= current->scopeDepth) {
			int jumpLenght = curCode - curContinue.offset - 4;
			int toPop = curContinue.varNum - current->localCount;
			//variables declared by the time we hit the break whose depth is lower or equal to this break stmt
			getChunk()->code[curContinue.offset] = (toPop >> 8) & 0xff;
			getChunk()->code[curContinue.offset + 1] = toPop & 0xff;
			//amount to jump
			getChunk()->code[curContinue.offset + 2] = (jumpLenght >> 8) & 0xff;
			getChunk()->code[curContinue.offset + 3] = jumpLenght & 0xff;
			//delete break from array
			current->continueStmts.pop_back();
		}
		else break;
	}
}


Token compiler::syntheticToken(const char* str) {
	return Token(str, current->line, TOKEN_IDENTIFIER);
}

#pragma endregion

#pragma region Classes and methods
void compiler::method(ASTFunc* _method, Token className) {
	updateLine(_method->getName());
	uInt16 name = identifierConstant(_method->getName());
	//creating a new compilerInfo sets us up with a clean slate for writing bytecode, the enclosing functions info
	//is stored in current->enclosing
	funcType type = funcType::TYPE_METHOD;
	//constructors are treated separatly, but are still methods
	if (_method->getName().getLexeme().compare(className.getLexeme()) == 0) type = funcType::TYPE_CONSTRUCTOR;
	current = new compilerInfo(current, type);
	//->func = new objFunc();
	//no need for a endScope, since returning from the function discards the entire callstack
	beginScope();
	//we define the args as locals, when the function is called, the args will be sitting on the stack in order
	//we just assign those positions to each arg
	for (Token& var : _method->getArgs()) {
		uInt16 constant = parseVar(var);
		defineVar(constant);
	}
	_method->getBody()->accept(this);
	current->func->arity = _method->getArgs().size();

	string str = _method->getName().getLexeme();
	current->func->name = copyString((char*)str.c_str(), str.length());
	//have to do this here since endFuncDecl() deletes the compilerInfo
	std::array<upvalue, UPVAL_MAX> upvals = current->upvalues;

	objFunc* func = endFuncDecl();
	if (func->upvalueCount == 0) {
		gc.cachePtr(func);
		objClosure* closure = new objClosure(dynamic_cast<objFunc*>(gc.getCachedPtr()));
		emitConstant(OBJ_VAL(closure));

		emitByteAnd16Bit(OP_METHOD, name);
		return;
	}
	uInt16 constant = makeConstant(OBJ_VAL(func));

	if (constant < UINT8_MAX) emitBytes(OP_CLOSURE, constant);
	else emitByteAnd16Bit(OP_CLOSURE_LONG, constant);
	//if this function does capture any upvalues, we emit the code for getting them, 
	//when we execute "OP_CLOSURE" we will check to see how many upvalues the function captures by going directly to the func->upvalueCount
	for (int i = 0; i < func->upvalueCount; i++) {
		emitByte(upvals[i].isLocal ? 1 : 0);
		emitByte(upvals[i].index);
	}
	emitByteAnd16Bit(OP_METHOD, name);
}

bool compiler::invoke(ASTCallExpr* expr) {
	//we only optimize function calls
	if (expr->getAccessor().type != TOKEN_LEFT_PAREN) return false;
	if (expr->getCallee()->type == ASTType::CALL) {
		//currently we only optimizes field invoking(struct.field())
		ASTCallExpr* _call = (ASTCallExpr*)expr->getCallee();
		if (_call->getAccessor().type != TOKEN_DOT) return false;

		_call->getCallee()->accept(this);
		uInt16 field = identifierConstant(((ASTLiteralExpr*)_call->getArgs()[0])->getToken());
		int argCount = 0;
		for (ASTNode* arg : expr->getArgs()) {
			arg->accept(this);
			argCount++;
		}
		if (field < UINT8_MAX) {
			emitBytes(OP_INVOKE, field);
			emitByte(argCount);
		}
		else {
			emitByteAnd16Bit(OP_INVOKE_LONG, field);
			emitByte(argCount);
		}
		return true;
	}
	else if (expr->getCallee()->type == ASTType::SUPER) {
		ASTSuperExpr* _superCall = (ASTSuperExpr*)expr->getCallee();
		int name = identifierConstant(_superCall->getName());

		if (currentClass == nullptr) {
			error(_superCall->getName(), "Can't use 'super' outside of a class.");
		}
		else if (!currentClass->hasSuperclass) {
			error(_superCall->getName(), "Can't use 'super' in a class with no superclass.");
		}

		namedVar(syntheticToken("this"), false);
		int argCount = 0;
		for (ASTNode* arg : expr->getArgs()) {
			arg->accept(this);
			argCount++;
		}
		//super gets popped, leaving only the receiver and args on the stack
		namedVar(syntheticToken("super"), false);
		if (name < UINT8_MAX) {
			emitBytes(OP_SUPER_INVOKE, name);
			emitByte(argCount);
		}
		else {
			emitByteAnd16Bit(OP_SUPER_INVOKE_LONG, name);
			emitByte(argCount);
		}
		return true;
	}
	return false;
}
#pragma endregion


Chunk* compiler::getChunk() {
	return &current->func->body;
}

void compiler::error(string message) {
	errorHandler::addSystemError("System compile error [line " + current->line + "] in '" + curUnit->name + "': \n" + message + "\n");
	throw CompilerException();
}

void Compiler::error(Token token, string msg) {
	errorHandler::addCompilerError(msg, token);
	throw CompilerException();
}

objFunc* compiler::endFuncDecl() {
	if (!current->hasReturn) emitReturn();
	//get the current function we've just compiled, delete it's compiler info, and replace it with the enclosing functions compiler info
	objFunc* func = current->func;
	//for the last line of code
	func->body.lines[func->body.lines.size() - 1].end = func->body.code.count();
#ifdef DEBUG_PRINT_CODE
	current->func->body.disassemble(current->func->name == nullptr ? "script" : current->func->name->str);
#endif
	compilerInfo* temp = current->enclosing;
	delete current;
	current = temp;
	return func;
}

//a little helper for updating the lines emitted by the compiler(used for displaying runtime errors)
void Compiler::updateLine(Token token) {
	current->line = token.line;
}
#pragma endregion


