#include "compiler.h"
#include "../MemoryManagment/garbageCollector.h"
#include "../ErrorHandling/errorHandler.h"
#include <unordered_set>

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


Compiler::Compiler(vector<CSLModule*>& _units) {
	current = new CurrentChunkInfo(nullptr, funcType::TYPE_SCRIPT);
	currentClass = nullptr;
	vector<File*> sourceFiles;
	curUnitIndex = 0;
	units = _units;

	for (CSLModule* unit : units) {
		vector<Token> depAlias;
		//for every unit we check every dependency and see if there are any repeating aliases in the same unit
		for (Dependency dep : unit->deps) {
			try {
				if (dep.alias.type != TokenType::NONE) {
					for (Token& token : depAlias) {
						if (token.compare(dep.alias)) {
							error(token, "Cannot use the same alias for 2 module imports.");
							error(dep.alias, "Cannot use the same alias for 2 module imports.");
						}
					}
					depAlias.push_back(dep.alias);
				}
			}
			catch (CompilerException e) {
				continue;
			}
		}
	}

	for (CSLModule* unit : units) {
		curUnit = unit;
		sourceFiles.push_back(unit->file);
		for (int i = 0; i < unit->stmts.size(); i++) {
			//doing this here so that even if a error is detected, we go on and possibly catch other(valid) errors
			try {
				unit->stmts[i]->accept(this);
			}
			catch (CompilerException e) {
				
			}
		}
		curUnitIndex++;
	}
	for (CSLModule* unit : units) delete unit;
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

void Compiler::visitFuncLiteral(AST::FuncLiteral* expr) {;
	//creating a new compilerInfo sets us up with a clean slate for writing bytecode, the enclosing functions info
	//is stored in current->enclosing
	current = new CurrentChunkInfo(current, funcType::TYPE_FUNC);
	//current->func = new objFunc();
	//no need for a endScope, since returning from the function discards the entire callstack
	beginScope();
	//we define the args as locals, when the function is called, the args will be sitting on the stack in order
	//we just assign those positions to each arg
	for (Token& var : expr->args) {
		updateLine(var);
		uint8_t constant = parseVar(var);
		defineVar(constant);
	}
	expr->body->accept(this);
	current->func->arity = expr->args.size();
	char str[] = "Anonymous function";
	current->func->name = ObjString::createString(str, strlen(str), internedStrings);
	//have to do this here since endFuncDecl() deletes the compilerInfo
	std::array<Upvalue, UPVAL_MAX> upvals = current->upvalues;

	ObjFunc* func = endFuncDecl();
	if (func->upvalueCount == 0) {
		ObjClosure* closure = new ObjClosure(dynamic_cast<ObjFunc*>(func));
		emitConstant(Value(closure));
		return;
	}

	uInt16 constant = makeConstant(Value(func));
	if (constant < UINT8_MAX) emitBytes(+OpCode::CLOSURE, constant);
	else emitByteAnd16Bit(+OpCode::CLOSURE_LONG, constant);
	//if this function does capture any upvalues, we emit the code for getting them, 
	//when we execute "OP_CLOSURE" we will check to see how many upvalues the function captures by going directly to the func->upvalueCount
	for (int i = 0; i < func->upvalueCount; i++) {
		emitByte(upvals[i].isLocal ? 1 : 0);
		emitByte(upvals[i].index);
	}
}

void Compiler::visitModuleAccessExpr(AST::ModuleAccessExpr* expr) {
	Dependency* depPtr = nullptr;
	for (Dependency dep : curUnit->deps) {
		if (dep.alias.compare(expr->moduleName)) {
			depPtr = &dep;
			break;
		}
	}
	if (depPtr == nullptr) {
		error(expr->moduleName, "Module alias doesn't exist.");
	}
	CSLModule* unit = depPtr->module;
	for (Token& token : unit->exports) {
		if (token.compare(expr->ident)) {
			int i = 0;
			for (i = 0; i < units.size(); i++) if (units[i] == unit) break;

			string temp = std::to_string(i) + expr->ident.getLexeme();
			uInt arg = makeConstant(Value(ObjString::createString((char*)temp.c_str(), temp.length(), internedStrings)));

			if (arg > UINT8_MAX) {
				emitByteAnd16Bit(+OpCode::GET_GLOBAL_LONG, arg);
				return;
			}
			emitBytes(+OpCode::GET_GLOBAL, arg);
			return;
		}
	}
	error(expr->ident, std::format("Module {} doesn't export this symbol.", depPtr->alias.getLexeme()));
}

//TODO: do this when implementing multithreading
void Compiler::visitAwaitExpr(AST::AwaitExpr* expr) {

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
	decl->body->accept(this);
	current->func->arity = decl->args.size();
	//could get away with using string instead of objString?
	string str = decl->getName().getLexeme();
	current->func->name = ObjString::createString((char*)str.c_str(), str.length(), internedStrings);
	//have to do this here since endFuncDecl() deletes the compilerInfo
	std::array<Upvalue, UPVAL_MAX> upvals = current->upvalues;
	
	ObjFunc* func = endFuncDecl();
	if (func->upvalueCount == 0) {
		ObjClosure* closure = new ObjClosure(dynamic_cast<ObjFunc*>(func));
		emitConstant(Value(closure));
		defineVar(name);
		return;
	}

	uInt16 constant = makeConstant(Value(func));
	if (constant < UINT8_MAX) emitBytes(+OpCode::CLOSURE, constant);
	else emitByteAnd16Bit(+OpCode::CLOSURE_LONG, constant);
	//if this function does capture any upvalues, we emit the code for getting them, 
	//when we execute "OP_CLOSURE" we will check to see how many upvalues the function captures by going directly to the func->upvalueCount
	for (int i = 0; i < func->upvalueCount; i++) {
		emitByte(upvals[i].isLocal ? 1 : 0);
		emitByte(upvals[i].index);
	}
	defineVar(name);
}

void Compiler::visitClassDecl(AST::ClassDecl* decl) {
	Token className = decl->getName();
	uInt16 constant = identifierConstant(className);
	declareVar(className);

	emitByteAnd16Bit(+OpCode::CLASS, constant);

	//define the class here, so that we can use it inside it's own methods
	defineVar(constant);

	ClassChunkInfo temp(currentClass, false);
	currentClass = &temp;

	if (decl->inherits) {
		//if the class does inherit from some other class, we load the parent class and declare 'super' as a local variable
		//which holds said class
		namedVar(decl->inheritedClass, false);
		if (className.getLexeme().compare(decl->inheritedClass.getLexeme()) == 0) {
			error(decl->inheritedClass, "A class can't inherit from itself.");
		}
		beginScope();
		addLocal(syntheticToken("super"));
		defineVar(0);

		namedVar(className, false);
		emitByte(+OpCode::INHERIT);
		currentClass->hasSuperclass = true;
	}
	//we need to load the class onto the top of the stack so that 'this' keyword can work correctly inside of methods
	//the class that 'this' refers to is captured as a upvalue inside of methods
	if (!decl->inherits) namedVar(className, false);
	for (AST::ASTNodePtr _method : decl->methods) {
		method(dynamic_cast<AST::FuncDecl*>(_method.get()), className);
	}
	//pop the current class
	emitByte(+OpCode::POP);

	if (currentClass->hasSuperclass) {
		endScope();
	}
	currentClass = currentClass->enclosing;
}



void Compiler::visitPrintStmt(AST::PrintStmt* stmt) {
	stmt->expr->accept(this);
	//OP_TO_STRING is emitted first to handle the case of a instance whose class defines a toString method
	emitByte(+OpCode::PRINT);
}

void Compiler::visitExprStmt(AST::ExprStmt* stmt) {
	stmt->expr->accept(this);
	emitByte(+OpCode::POP);
}

void Compiler::visitBlockStmt(AST::BlockStmt* stmt) {
	beginScope();
	for (AST::ASTNodePtr node : stmt->statements) {
		node->accept(this);
	}
	endScope();
}

void Compiler::visitIfStmt(AST::IfStmt* stmt) {
	//compile condition and emit a jump over then branch if the condition is false
	stmt->condition->accept(this);
	int thenJump = emitJump(+OpCode::JUMP_IF_FALSE_POP);
	stmt->thenBranch->accept(this);
	//only compile if there is a else branch
	if (stmt->elseBranch != nullptr) {
		//prevents fallthrough to else branch
		int elseJump = emitJump(+OpCode::JUMP);
		patchJump(thenJump);

		stmt->elseBranch->accept(this);
		patchJump(elseJump);
	}
	else patchJump(thenJump);

}

void Compiler::visitWhileStmt(AST::WhileStmt* stmt) {
	//the bytecode for this is almost the same as if statement
	//but at the end of the body, we loop back to the start of the condition
	int loopStart = getChunk()->code.size();
	stmt->condition->accept(this);
	int jump = emitJump(+OpCode::JUMP_IF_FALSE_POP);
	stmt->body->accept(this);
	patchScopeJumps(ScopeJumpType::CONTINUE);
	emitLoop(loopStart);
	patchJump(jump);
	patchScopeJumps(ScopeJumpType::BREAK);
}

void Compiler::visitForStmt(AST::ForStmt* stmt) {
	//we wrap this in a scope so if there is a var declaration in the initialization it's scoped to the loop
	beginScope();
	if (stmt->init != nullptr) stmt->init->accept(this);
	int loopStart = getChunk()->code.size();
	//only emit the exit jump code if there is a condition expression
	int exitJump = -1;
	if (stmt->condition != nullptr) {
		stmt->condition->accept(this);
		exitJump = emitJump(+OpCode::JUMP_IF_FALSE_POP);
	}
	//body is mandatory
	stmt->body->accept(this);
	//patching continue here to increment if a variable for incrementing has been defined
	patchScopeJumps(ScopeJumpType::CONTINUE);
	//if there is a increment expression, we compile it and emit a POP to get rid of the result
	if (stmt->increment != nullptr) {
		stmt->increment->accept(this);
		emitByte(+OpCode::POP);
	}
	emitLoop(loopStart);
	//exit jump still needs to handle scoping appropriately 
	if (exitJump != -1) patchJump(exitJump);
	endScope();
	//patch breaks AFTER we close the for scope because breaks that are in current scope aren't patched
	patchScopeJumps(ScopeJumpType::BREAK);
}

void Compiler::visitBreakStmt(AST::BreakStmt* stmt) {
	//the amount of variables to pop and the amount of code to jump is determined in patchScopeJumps()
	//which is called at the end of loops or a switch
	updateLine(stmt->token);
	emitByte(+ScopeJumpType::BREAK);
	int breakJump = getChunk()->code.size();
	emitBytes((current->scopeDepth >> 8) & 0xff, current->scopeDepth & 0xff);
	emitBytes((current->localCount >> 8) & 0xff, current->localCount & 0xff);
	current->scopeJumps.push_back(breakJump);
}

void Compiler::visitContinueStmt(AST::ContinueStmt* stmt) {
	//the amount of variables to pop and the amount of code to jump is determined in patchScopeJumps()
	//which is called at the end of loops
	updateLine(stmt->token);
	emitByte(+ScopeJumpType::CONTINUE);
	int continueJump = getChunk()->code.size();
	emitBytes((current->scopeDepth >> 8) & 0xff, current->scopeDepth & 0xff);
	emitBytes((current->localCount >> 8) & 0xff, current->localCount & 0xff);
	current->scopeJumps.push_back(continueJump);
}

void Compiler::visitSwitchStmt(AST::SwitchStmt* stmt) {
	beginScope();
	//compile the expression in parentheses
	stmt->expr->accept(this);
	vector<uInt16> constants;
	vector<uInt16> jumps;
	bool isLong = false;
	for (std::shared_ptr<AST::CaseStmt> _case : stmt->cases) {
		if (_case->caseType.getLexeme().compare("default")) continue;
		for (Token constant : _case->constants) {
			Value val;
			updateLine(constant);
			//create constant and add it to the constants array
			try {
				switch (constant.type) {
					case TokenType::NUMBER: {
						double num = std::stod(constant.getLexeme());//doing this becuase stod doesn't accept string_view
						val = Value(num);
						break;
					}
					case TokenType::TRUE: val = Value(true); break;
					case TokenType::FALSE: val = Value(false); break;
					case TokenType::NIL: val = Value::nil(); break;
					case TokenType::STRING: {
						//this gets rid of quotes, "Hello world"->Hello world
						string temp = constant.getLexeme();
						temp.erase(0, 1);
						temp.erase(temp.size() - 1, 1);
						val = Value(ObjString::createString((char*)temp.c_str(), temp.length(), internedStrings));
						break;
					}
					default: {
						error(constant, "Case expression can only be a constant.");
					}
				}
				constants.push_back(makeConstant(val));
				if (constants.back() > UINT8_MAX) isLong = true;
			}
			catch (CompilerException e) {

			}
		}
	}
	//the arguments for a switch op code are:
	//8-bit number n of case constants
	//n 8 or 16 bit numbers for each constant
	//n + 1 16-bit numbers of jump offsets(default case is excluded from constants, so the number of jumps is the number of constants + 1)
	//the default jump offset is always the last
	if (isLong) {
		emitBytes(+OpCode::SWITCH_LONG, constants.size());
		for (uInt16 constant : constants) {
			emit16Bit(constant);
		}
	}
	else {
		emitBytes(+OpCode::SWITCH, constants.size());
		for (uInt16 constant : constants) {
			emitByte(constant);
		}
	}
	
	for (int i = 0; i < constants.size(); i++) {
		jumps.push_back(getChunk()->code.size());
		emit16Bit(0xffff);
	}
	//default jump
	jumps.push_back(getChunk()->code.size());
	emit16Bit(0xffff);

	//at the end of each case is a implicit break
	vector<uInt> implicitBreaks;

	//compile the code of all cases, before each case update the jump for that case to the current ip
	int i = 0;
	for (std::shared_ptr<AST::CaseStmt> _case : stmt->cases) {
		if (_case->caseType.getLexeme().compare("default")) {
			jumps[jumps.size() - 1] = getChunk()->code.size();
		}
		else {
			for (Token token : _case->constants) {
				jumps[i] = getChunk()->code.size();
				i++;
			}
		}
		patchScopeJumps(ScopeJumpType::ADVANCE);
		beginScope();
		_case->accept(this);
		endScope();
		implicitBreaks.push_back(emitJump(+OpCode::JUMP));
	}
	//if there is no default case the default jump goes to the end of the switch stmt
	if (!stmt->hasDefault) jumps[jumps.size() - 1] = getChunk()->code.size();

	//all implicit breaks lead to the end of the switch statement
	for (uInt jmp : implicitBreaks) {
		patchJump(jmp);
	}

	//we use a scope and patch breaks AFTER ending the scope because breaks that are in the current scope aren't patched
	endScope();
	patchScopeJumps(ScopeJumpType::BREAK);
}

void Compiler::visitCaseStmt(AST::CaseStmt* stmt) {
	//compile every statement in the case
	//user has to worry about fallthrough
	for (AST::ASTNodePtr stmt : stmt->stmts) {
		stmt->accept(this);
	}
}

void Compiler::visitAdvanceStmt(AST::AdvanceStmt* stmt) {
	//the amount of variables to pop and the amount of code to jump is determined in patchScopeJumps()
	//which is called at the start of each case statements
	updateLine(stmt->token);
	emitByte(+ScopeJumpType::ADVANCE);
	int advanceJump = getChunk()->code.size();
	emitBytes((current->scopeDepth >> 8) & 0xff, current->scopeDepth & 0xff);
	emitBytes((current->localCount >> 8) & 0xff, current->localCount & 0xff);
	current->scopeJumps.push_back(advanceJump);
}

void Compiler::visitReturnStmt(AST::ReturnStmt* stmt) {
	updateLine(stmt->keyword);
	if (current->type == funcType::TYPE_SCRIPT) {
		error(stmt->keyword, "Can't return from top-level code.");
	}
	else if (current->type == funcType::TYPE_CONSTRUCTOR) {
		error(stmt->keyword, "Can't return a value from a constructor.");
	}
	if (stmt->expr == nullptr) {
		emitReturn();
		return;
	}
	stmt->expr->accept(this);
	emitByte(+OpCode::RETURN);
	current->hasReturnStmt = true;
}

#pragma region helpers

#pragma region Emitting bytes

void Compiler::emitByte(uint8_t byte) {
	getChunk()->writeData(byte, current->line, curUnit->file->name);//line is incremented whenever we find a statement/expression that contains tokens
}

void Compiler::emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

void Compiler::emit16Bit(uInt16 number) {
	//Big endian
	emitBytes((number >> 8) & 0xff, number & 0xff);
}

void Compiler::emitByteAnd16Bit(uint8_t byte, uInt16 num) {
	emitByte(byte);
	emit16Bit(num);
}

uInt16 Compiler::makeConstant(Value value) {
	uInt16 constant = getChunk()->addConstant(value);
	if (constant > UINT16_MAX) {
		error("Too many constants in one chunk.");
		return 0;
	}
	return constant;
}

void Compiler::emitConstant(Value value) {
	//shorthand for adding a constant to the chunk and emitting it
	uInt16 constant = makeConstant(value);
	if (constant < 256) emitBytes(+OpCode::CONSTANT, constant);
	else if (constant < UINT16_MAX) emitByteAnd16Bit(+OpCode::CONSTANT_LONG, constant);
	else error("Constant overflow");
}

void Compiler::emitGlobalVar(Token name, bool canAssign) {
	//all global variables have a numerical prefix which indicates which source file they came from, used for scoping
	string temp = resolveGlobal(name, canAssign);
	uInt arg = makeConstant(Value(ObjString::createString((char*)temp.c_str(), temp.length(), internedStrings)));

	uint8_t getOp = +OpCode::GET_GLOBAL;
	uint8_t setOp = +OpCode::SET_GLOBAL;
	if (arg > UINT8_MAX) {
		getOp = +OpCode::GET_GLOBAL_LONG;
		setOp = +OpCode::SET_GLOBAL_LONG;
		emitByteAnd16Bit(canAssign ? setOp : getOp, arg);
		return;
	}
	emitByte(canAssign ? setOp : getOp);
	emitByte(arg);
}

void Compiler::emitReturn() {
	if (current->type == funcType::TYPE_CONSTRUCTOR) emitBytes(+OpCode::GET_LOCAL, 0);
	else emitByte(+OpCode::NIL);
	emitByte(+OpCode::RETURN);
}

int Compiler::emitJump(uint8_t jumpType) {
	emitByte(jumpType);
	emitBytes(0xff, 0xff);
	return getChunk()->code.size() - 2;
}

void Compiler::patchJump(int offset) {
	// -2 to adjust for the bytecode for the jump offset itself.
	int jump = getChunk()->code.size() - offset - 2;
	//fix for future: insert 2 more bytes into the array, but make sure to do the same in lines array
	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}
	getChunk()->code[offset] = (jump >> 8) & 0xff;
	getChunk()->code[offset + 1] = jump & 0xff;
}

void Compiler::emitLoop(int start) {
	emitByte(+OpCode::LOOP);

	int offset = getChunk()->code.size() - start + 2;
	if (offset > UINT16_MAX) error("Loop body too large.");

	emit16Bit(offset);
}

void Compiler::patchScopeJumps(ScopeJumpType type) {
	int curCode = getChunk()->code.size();
	//most recent jumps are going to be on top
	for (int i = current->scopeJumps.size() - 1; i >= 0; i--) {
		uInt jumpPatchPos = current->scopeJumps[i];
		byte jumpType = getChunk()->code[jumpPatchPos - 1];
		uInt jumpDepth = (getChunk()->code[jumpPatchPos] << 8) | getChunk()->code[jumpPatchPos + 1];
		uInt jumpVarNum = (getChunk()->code[jumpPatchPos + 2] << 8) | getChunk()->code[jumpPatchPos + 3];
		//break and advance statements which are in a strictly deeper scope get patched, on the other hand
		//continue statements which are in current or a deeper scope get patched
		if (((jumpDepth > current->scopeDepth && type == ScopeJumpType::BREAK) ||
			(jumpDepth >= current->scopeDepth && type == ScopeJumpType::CONTINUE) ||
			(jumpDepth > current->scopeDepth && type == ScopeJumpType::ADVANCE)) && +type == jumpType) {
			int jumpLenght = curCode - jumpPatchPos - 4;
			int toPop = jumpVarNum - current->localCount;
			if (jumpLenght > UINT16_MAX) error("Too much code to jump over.");
			if (toPop > UINT16_MAX) error("Too many variables to pop.");

			getChunk()->code[jumpPatchPos - 1] = +OpCode::JUMP_POPN;
			//variables declared by the time we hit the break whose depth is lower or equal to this break stmt
			getChunk()->code[jumpPatchPos] = (toPop >> 8) & 0xff;
			getChunk()->code[jumpPatchPos + 1] = toPop & 0xff;
			//amount to jump
			getChunk()->code[jumpPatchPos + 2] = (jumpLenght >> 8) & 0xff;
			getChunk()->code[jumpPatchPos + 3] = jumpLenght & 0xff;

			current->scopeJumps.erase(current->scopeJumps.begin() + i);
		}
		else break;
	}
}

#pragma endregion

#pragma region Variables

bool identifiersEqual(const string& a, const string& b) {
	return (a.compare(b) == 0);
}

uInt16 Compiler::identifierConstant(Token name) {
	updateLine(name);
	string temp = name.getLexeme();
	return makeConstant(Value(ObjString::createString((char*)temp.c_str(), temp.length(), internedStrings)));
}

void Compiler::defineVar(uInt16 name) {
	//if this is a local var, mark it as ready and then bail out
	if (current->scopeDepth > 0) {
		markInit();
		return;
	}
	if (name < UINT8_MAX) emitBytes(+OpCode::DEFINE_GLOBAL, name);
	else {
		emitByteAnd16Bit(+OpCode::DEFINE_GLOBAL_LONG, name);
	}
}

void Compiler::namedVar(Token token, bool canAssign) {
	updateLine(token);
	uint8_t getOp;
	uint8_t setOp;
	uInt arg = resolveLocal(token);
	if (arg != -1) {
		getOp = +OpCode::GET_LOCAL;
		setOp = +OpCode::SET_LOCAL;
	}
	else if ((arg = resolveUpvalue(current, token)) != -1) {
		getOp = +OpCode::GET_UPVALUE;
		setOp = +OpCode::SET_UPVALUE;
	}
	else {
		return emitGlobalVar(token, canAssign);
	}
	emitBytes(canAssign ? setOp : getOp, arg);
}

//if 'name' is a global variable it's parsed and a string constant is returned
//otherwise, if 'name' is a local variable it's passed to declareVar()
uInt16 Compiler::parseVar(Token name) {
	updateLine(name);
	declareVar(name);
	if (current->scopeDepth > 0) return 0;
	//if this is a global variable, we tack on the index of this CSLModule at the start of the variable name(variable names can't start with numbers)
	//used to differentiate variables of the same name from different souce files

	//tacking on the current index since parseVar is only used for declaring a variable, which can only be done in the current source file
	string temp = std::to_string(curUnitIndex) +  name.getLexeme();
	return makeConstant(Value(ObjString::createString((char*)temp.c_str(), temp.length(), internedStrings)));
}

//makes sure the compiler is aware that a stack slot is occupied by this local variable
void Compiler::declareVar(Token& name) {
	updateLine(name);
	//if we are currently in global scope, this has no use
	if (current->scopeDepth == 0) return;
	for (int i = current->localCount - 1; i >= 0; i--) {
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth) {
			break;
		}
		string str = name.getLexeme();
		if (identifiersEqual(str, local->name)) {
			error(name, "Already a variable with this name in this scope.");
		}
	}
	addLocal(name);
}

void Compiler::addLocal(Token name) {
	updateLine(name);
	if (current->localCount == LOCAL_MAX) {
		error(name, "Too many local variables in function.");
		return;
	}
	Local* local = &current->locals[current->localCount++];
	local->name = name.getLexeme();
	local->depth = -1;
}

void Compiler::endScope() {
	//Pop every variable that was declared in this scope
	current->scopeDepth--;//first lower the scope, the check for every var that is deeper than the current scope
	int toPop = 0;
	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
		if (!current->hasCapturedLocals) toPop++;
		else {
			if (current->locals[current->localCount - 1].isCaptured) {
				emitByte(+OpCode::CLOSE_UPVALUE);
			}
			else {
				emitByte(+OpCode::POP);
			}
		}
		current->localCount--;
	}
	if (toPop > 0 && !current->hasCapturedLocals) emitBytes(+OpCode::POPN, toPop);
}

int Compiler::resolveLocal(CurrentChunkInfo* func, Token name) {
	//checks to see if there is a local variable with a provided name, if there is return the index of the stack slot of the var
	updateLine(name);
	for (int i = func->localCount - 1; i >= 0; i--) {
		Local* local = &func->locals[i];
		string str = name.getLexeme();
		if (identifiersEqual(str, local->name)) {
			if (local->depth == -1) {
				error(name, "Can't read local variable in its own initializer.");
			}
			return i;
		}
	}

	return -1;
}

int Compiler::resolveLocal(Token name) {
	return resolveLocal(current, name);
}

int Compiler::resolveUpvalue(CurrentChunkInfo* func, Token name) {
	if (func->enclosing == NULL) return -1;

	int local = resolveLocal(func->enclosing, name);
	if (local != -1) {
		func->enclosing->locals[local].isCaptured = true;
		func->enclosing->hasCapturedLocals = true;
		return addUpvalue((uint8_t)local, true);
	}
	int upvalue = resolveUpvalue(func->enclosing, name);
	if (upvalue != -1) {
		return addUpvalue((uint8_t)upvalue, false);
	}

	return -1;
}

int Compiler::addUpvalue(uint8_t index, bool isLocal) {
	int upvalueCount = current->func->upvalueCount;
	for (int i = 0; i < upvalueCount; i++) {
		Upvalue* upval = &current->upvalues[i];
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

void Compiler::markInit() {
	if (current->scopeDepth == 0) return;
	//marks variable as ready to use, any use of it before this call is a error
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

Token Compiler::syntheticToken(string str) {
	return Token(TokenType::IDENTIFIER, str);
}

#pragma endregion

#pragma region Classes and methods
void Compiler::method(AST::FuncDecl* _method, Token className) {
	updateLine(_method->getName());
	uInt16 name = identifierConstant(_method->getName());
	//creating a new compilerInfo sets us up with a clean slate for writing bytecode, the enclosing functions info
	//is stored in current->enclosing
	funcType type = funcType::TYPE_METHOD;
	//constructors are treated separatly, but are still methods
	if (_method->getName().getLexeme().compare(className.getLexeme()) == 0) type = funcType::TYPE_CONSTRUCTOR;
	current = new CurrentChunkInfo(current, type);
	//->func = new objFunc();
	//no need for a endScope, since returning from the function discards the entire callstack
	beginScope();
	//we define the args as locals, when the function is called, the args will be sitting on the stack in order
	//we just assign those positions to each arg
	for (Token& var : _method->args) {
		uInt16 constant = parseVar(var);
		defineVar(constant);
	}
	_method->body->accept(this);
	current->func->arity = _method->arity;

	string str = _method->getName().getLexeme();
	current->func->name = ObjString::createString((char*)str.c_str(), str.length(), internedStrings);
	//have to do this here since endFuncDecl() deletes the compilerInfo
	std::array<Upvalue, UPVAL_MAX> upvals = current->upvalues;

	ObjFunc* func = endFuncDecl();
	if (func->upvalueCount == 0) {
		ObjClosure* closure = new ObjClosure(dynamic_cast<ObjFunc*>(func));
		emitConstant(Value(closure));
		emitByteAnd16Bit(+OpCode::METHOD, name);
		return;
	}
	uInt16 constant = makeConstant(Value(func));

	if (constant < UINT8_MAX) emitBytes(+OpCode::CLOSURE, constant);
	else emitByteAnd16Bit(+OpCode::CLOSURE_LONG, constant);
	//if this function does capture any upvalues, we emit the code for getting them, 
	//when we execute "OP_CLOSURE" we will check to see how many upvalues the function captures by going directly to the func->upvalueCount
	for (int i = 0; i < func->upvalueCount; i++) {
		emitByte(upvals[i].isLocal ? 1 : 0);
		emitByte(upvals[i].index);
	}
	emitByteAnd16Bit(+OpCode::METHOD, name);
}

bool Compiler::invoke(AST::CallExpr* expr) {
	if (dynamic_cast<AST::FieldAccessExpr*>(expr->callee.get())) {
		//currently we only optimizes field invoking(struct.field())
		AST::FieldAccessExpr* call = dynamic_cast<AST::FieldAccessExpr*>(expr->callee.get());
		call->callee->accept(this);
		call->field->accept(this);
		int argCount = 0;
		for (AST::ASTNodePtr arg : expr->args) {
			arg->accept(this);
			argCount++;
		}
		emitBytes(+OpCode::INVOKE, argCount);
		return true;
	}
	else if (dynamic_cast<AST::SuperExpr*>(expr->callee.get())) {
		AST::SuperExpr* superCall = dynamic_cast<AST::SuperExpr*>(expr->callee.get());
		int name = identifierConstant(superCall->methodName);

		if (currentClass == nullptr) {
			error(superCall->methodName, "Can't use 'super' outside of a class.");
		}
		else if (!currentClass->hasSuperclass) {
			error(superCall->methodName, "Can't use 'super' in a class with no superclass.");
		}

		namedVar(syntheticToken("this"), false);
		int argCount = 0;
		for (AST::ASTNodePtr arg : expr->args) {
			arg->accept(this);
			argCount++;
		}
		//super gets popped, leaving only the receiver and args on the stack
		namedVar(syntheticToken("super"), false);
		emitConstant(getChunk()->constants[name]);
		emitBytes(+OpCode::INVOKE, argCount);
		return true;
	}
	return false;
}
#pragma endregion


Chunk* Compiler::getChunk() {
	return &current->func->body;
}

void Compiler::error(string message) {
	errorHandler::addSystemError("System compile error [line " + std::to_string(current->line) + "] in '" + curUnit->file->name + "': \n" + message + "\n");
	throw CompilerException();
}

void Compiler::error(Token token, string msg) {
	errorHandler::addCompileError(msg, token);
	throw CompilerException();
}

ObjFunc* Compiler::endFuncDecl() {
	if (!current->hasReturnStmt) emitReturn();
	//get the current function we've just compiled, delete it's compiler info, and replace it with the enclosing functions compiler info
	ObjFunc* func = current->func;
	//for the last line of code
	func->body.lines[func->body.lines.size() - 1].end = func->body.code.size();
#ifdef DEBUG_PRINT_CODE
	current->func->body.disassemble(current->func->name == nullptr ? "script" : current->func->name->str);
#endif
	CurrentChunkInfo* temp = current->enclosing;
	delete current;
	current = temp;
	return func;
}

//a little helper for updating the lines emitted by the compiler(used for displaying runtime errors)
void Compiler::updateLine(Token token) {
	current->line = token.str.line;
}

int Compiler::checkSymbol(Token symbol) {
	std::unordered_map<string, CSLModule*> importedSymbols;
	string lexeme = symbol.getLexeme();
	for (Dependency dep : curUnit->deps) {
		if (dep.alias.type == TokenType::NONE) {
			for (Token token : dep.module->exports) {
				if (token.getLexeme().compare(lexeme) != 0) continue;

				if (importedSymbols.count(lexeme) > 0) {
					string str = std::format("Ambiguous definition, symbol '{}' defined in {} and {}.", 
						lexeme, importedSymbols[lexeme]->file->name, dep.module->file->name);
					error(symbol, str);
				}
				else {
					importedSymbols[lexeme] = dep.module;
				}
			}
		}
	}
	if (importedSymbols.count(lexeme) == 0) {
		error(symbol, "Variable not defined.");
	}
	else {
		CSLModule* dep = importedSymbols[lexeme];
		for (int i = 0; i < units.size(); i++) {
			if (units[i] == dep) return i;
		}
		error(symbol, "Couldn't find source file of the definition.");
	}
}

string Compiler::resolveGlobal(Token name, bool canAssign) {
	bool inThisFile = false;
	for (Token token : curUnit->topDeclarations) {
		if (name.getLexeme().compare(name.getLexeme()) == 0) {
			inThisFile = true;
			break;
		}
	}
	if (canAssign) {
		if (inThisFile) return std::to_string(curUnitIndex) + name.getLexeme();
	}
	else {
		if (inThisFile) return std::to_string(curUnitIndex) + name.getLexeme();
		else {
			int i = checkSymbol(name);
			return std::to_string(i) + name.getLexeme();
		}
	}
	error(name, "Variable isn't declared.");
}
#pragma endregion


