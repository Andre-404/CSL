#pragma once
#include "codegenDefs.h"
#include "../Objects/objects.h"
#include "../Parsing/ASTDefs.h"
#include "../Parsing/parser.h"
#include <array>

namespace compileCore {
	#define LOCAL_MAX 256
	#define UPVAL_MAX 256

	enum class funcType {
		TYPE_FUNC,
		TYPE_METHOD,
		TYPE_CONSTRUCTOR,
		TYPE_SCRIPT,
	};

	struct Local {
		string name = "";
		int depth = -1;
		bool isCaptured = false;//whether this local variable has been captured as an upvalue
	};

	struct Upvalue {
		uint8_t index = 0;
		bool isLocal = false;
	};
	//used when compiling break and continue statements, depth is used to calculate how many local variables need to be popped
	//since we can jump over several scopes when eg. breaking out of a for loop
	//offset is how many bytes to jump over
	//varNum is how many variables need to be poppped from the stack
	struct ScopeJumpInfo {
		int depth = 0;
		int offset = -1;
		int varNum = 0;
		ScopeJumpInfo(int _d, int _o, int _varNum) : depth(_d), offset(_o), varNum(_varNum) {}
	};

	enum class ScopeJumpType {
		BREAK,
		CONTINUE,
		ADVANCE
	};
	//conversion from enum to 1 byte number
	inline constexpr unsigned operator+ (ScopeJumpType const val) { return static_cast<byte>(val); }


	//information about the current code chunk we're compiling, contains a reference to the enclosing code chunk which created this one
	struct CurrentChunkInfo {
		//for closures
		CurrentChunkInfo* enclosing;
		//function that's currently being compiled
		object::ObjFunc* func;
		funcType type;
		bool hasReturnStmt;

		uInt line;
		//information about unpatched 'continue' and 'break' statements
		vector<uInt> scopeJumps;
		//keeps track of every break and continue statement that has been encountered
		vector<ScopeJumpInfo> breakStmts;
		vector<ScopeJumpInfo> continueStmts;
		//locals
		Local locals[LOCAL_MAX];
		int localCount;
		int scopeDepth;
		std::array<Upvalue, UPVAL_MAX> upvalues;
		bool hasCapturedLocals;
		CurrentChunkInfo(CurrentChunkInfo* _enclosing, funcType _type);
	};

	struct ClassChunkInfo {
		ClassChunkInfo* enclosing;
		bool hasSuperclass;
		ClassChunkInfo(ClassChunkInfo* _enclosing, bool _hasSuperclass) : enclosing(_enclosing), hasSuperclass(_hasSuperclass) {};
	};

	struct CompilerException {

	};

	class Compiler : public AST::Visitor {
	public:
		//compiler only ever emits the code for a single function, top level code is considered a function
		CurrentChunkInfo* current;
		ClassChunkInfo* currentClass;
		//passed to the VM, used for highlighting runtime errors, managed by the VM
		vector<File*> sourceFiles;
		//interned strings
		HashMap internedStrings;
		Compiler(vector<CSLModule*>& units);
		Chunk* getChunk();
		object::ObjFunc* endFuncDecl();

		#pragma region Visitor pattern
		void visitAssignmentExpr(AST::AssignmentExpr* expr);
		void visitSetExpr(AST::SetExpr* expr);
		void visitConditionalExpr(AST::ConditionalExpr* expr);
		void visitBinaryExpr(AST::BinaryExpr* expr);
		void visitUnaryExpr(AST::UnaryExpr* expr);
		void visitCallExpr(AST::CallExpr* expr);
		void visitFieldAccessExpr(AST::FieldAccessExpr* expr);
		void visitGroupingExpr(AST::GroupingExpr* expr);
		void visitAwaitExpr(AST::AwaitExpr* expr);
		void visitArrayDeclExpr(AST::ArrayLiteralExpr* expr);
		void visitStructLiteralExpr(AST::StructLiteral* expr);
		void visitLiteralExpr(AST::LiteralExpr* expr);
		void visitSuperExpr(AST::SuperExpr* expr);
		void visitFuncLiteral(AST::FuncLiteral* expr);
		void visitModuleAccessExpr(AST::ModuleAccessExpr* expr);

		void visitVarDecl(AST::VarDecl* decl);
		void visitFuncDecl(AST::FuncDecl* decl);
		void visitClassDecl(AST::ClassDecl* decl);

		void visitPrintStmt(AST::PrintStmt* stmt);
		void visitExprStmt(AST::ExprStmt* stmt);
		void visitBlockStmt(AST::BlockStmt* stmt);
		void visitIfStmt(AST::IfStmt* stmt);
		void visitWhileStmt(AST::WhileStmt* stmt);
		void visitForStmt(AST::ForStmt* stmt);
		void visitBreakStmt(AST::BreakStmt* stmt);
		void visitContinueStmt(AST::ContinueStmt* stmt);
		void visitSwitchStmt(AST::SwitchStmt* stmt);
		void visitCaseStmt(AST::CaseStmt* _case);
		void visitAdvanceStmt(AST::AdvanceStmt* stmt);
		void visitReturnStmt(AST::ReturnStmt* stmt);
		#pragma endregion 
	private:
		CSLModule* curUnit;
		int curUnitIndex;
		vector<CSLModule*> units;

		#pragma region Helpers
		//emitters
		void emitByte(uint8_t byte);
		void emitBytes(uint8_t byte1, uint8_t byte2);
		void emit16Bit(uInt16 number);
		void emitByteAnd16Bit(uint8_t byte, uInt16 num);
		void emitConstant(Value value);
		void emitReturn();
		//control flow
		int emitJump(uint8_t jumpType);
		void patchJump(int offset);
		void emitLoop(int start);
		void patchScopeJumps(ScopeJumpType type);
		uInt16 makeConstant(Value value);
		//variables
		uInt16 identifierConstant(Token name);
		void defineVar(uInt16 name);
		void namedVar(Token name, bool canAssign);
		uInt16 parseVar(Token name);
		void emitGlobalVar(Token name, bool canAssign);
		//locals
		void declareVar(Token& name);
		void addLocal(Token name);
		int resolveLocal(Token name);
		int resolveLocal(CurrentChunkInfo* func, Token name);
		int resolveUpvalue(CurrentChunkInfo* func, Token name);
		int addUpvalue(uint8_t index, bool isLocal);
		void markInit();
		void beginScope() { current->scopeDepth++; }
		void endScope();
		//classes and methods
		void method(AST::FuncDecl* _method, Token className);
		bool invoke(AST::CallExpr* expr);
		Token syntheticToken(string str);
		//misc
		void updateLine(Token token);
		void error(Token token, string msg);
		void error(string message);
		//checks all imports to see if the symbol 'token' is imported
		int checkSymbol(Token token);
		//given a token and whether the operation is assigning or reading a variable, determines the correct symbol to use
		string resolveGlobal(Token token, bool canAssign);
		#pragma endregion
	};
}