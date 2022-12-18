#pragma once
#pragma once
#include "../modulesDefs.h"

namespace AST {
	using std::shared_ptr;
	class AssignmentExpr;
	class SetExpr;
	class ConditionalExpr;
	class BinaryExpr;
	class UnaryExpr;
	class ArrayLiteralExpr;
	class CallExpr;
	class FieldAccessExpr;
	class GroupingExpr;
	class AwaitExpr;
	class StructLiteral;
	class LiteralExpr;
	class SuperExpr;
	class FuncLiteral;
	class ModuleAccessExpr;

	class VarDecl;
	class FuncDecl;
	class ClassDecl;

	class PrintStmt;
	class ExprStmt;
	class BlockStmt;
	class IfStmt;
	class WhileStmt;
	class ForStmt;
	class BreakStmt;
	class ContinueStmt;
	class SwitchStmt;
	class CaseStmt;
	class ReturnStmt;

	//visitor pattern
	class Visitor {
	public:
		virtual void visitAssignmentExpr(AssignmentExpr* expr) = 0;
		virtual void visitSetExpr(SetExpr* expr) = 0;
		virtual void visitConditionalExpr(ConditionalExpr* expr) = 0;
		virtual void visitBinaryExpr(BinaryExpr* expr) = 0;
		virtual void visitUnaryExpr(UnaryExpr* expr) = 0;
		virtual void visitCallExpr(CallExpr* expr) = 0;
		virtual void visitFieldAccessExpr(FieldAccessExpr* expr) = 0;
		virtual void visitGroupingExpr(GroupingExpr* expr) = 0;
		virtual void visitAwaitExpr(AwaitExpr* expr) = 0;
		virtual void visitArrayDeclExpr(ArrayLiteralExpr* expr) = 0;
		virtual void visitStructLiteralExpr(StructLiteral* expr) = 0;
		virtual void visitLiteralExpr(LiteralExpr* expr) = 0;
		virtual void visitSuperExpr(SuperExpr* expr) = 0;
		virtual void visitFuncLiteral(FuncLiteral* expr) = 0;
		virtual void visitModuleAccessExpr(ModuleAccessExpr* expr) = 0;

		virtual void visitVarDecl(VarDecl* decl) = 0;
		virtual void visitFuncDecl(FuncDecl* decl) = 0;
		virtual void visitClassDecl(ClassDecl* decl) = 0;

		virtual void visitPrintStmt(PrintStmt* stmt) = 0;
		virtual void visitExprStmt(ExprStmt* stmt) = 0;
		virtual void visitBlockStmt(BlockStmt* stmt) = 0;
		virtual void visitIfStmt(IfStmt* stmt) = 0;
		virtual void visitWhileStmt(WhileStmt* stmt) = 0;
		virtual void visitForStmt(ForStmt* stmt) = 0;
		virtual void visitBreakStmt(BreakStmt* stmt) = 0;
		virtual void visitContinueStmt(ContinueStmt* stmt) = 0;
		virtual void visitSwitchStmt(SwitchStmt* stmt) = 0;
		virtual void visitCaseStmt(CaseStmt* _case) = 0;
		virtual void visitReturnStmt(ReturnStmt* stmt) = 0;
	};

	class ASTNode {
	public:
		virtual ~ASTNode() {};
		virtual void accept(Visitor* vis) = 0;
	};

	class ASTDecl : public ASTNode {
	public:
		virtual Token getName() = 0;
	};

	#pragma region Expressions

	class AssignmentExpr : public ASTNode {
	public:
		Token name;
		shared_ptr<ASTNode> value;

		AssignmentExpr(Token _name, shared_ptr<ASTNode> _value) {
			name = _name;
			value = _value;
		}
		void accept(Visitor* vis) {
			vis->visitAssignmentExpr(this);
		}
	};

	//used for assigning values to struct, class and array fields
	class SetExpr : public ASTNode {
	public:
		shared_ptr<ASTNode> callee;
		shared_ptr<ASTNode> field;
		Token accessor;
		Token op;
		shared_ptr<ASTNode> value;

		SetExpr(shared_ptr<ASTNode> _callee, shared_ptr<ASTNode> _field, Token _accessor, Token _op, shared_ptr<ASTNode> _val) {
			callee = _callee;
			field = _field;
			accessor = _accessor;
			value = _val;
			op = _op;
		}
		void accept(Visitor* vis) {
			vis->visitSetExpr(this);
		}
	};

	class ConditionalExpr : public ASTNode {
	public:
		shared_ptr<ASTNode> condition;
		shared_ptr<ASTNode> thenBranch;
		shared_ptr<ASTNode> elseBranch;

		ConditionalExpr(shared_ptr<ASTNode> _condition, shared_ptr<ASTNode> _thenBranch, shared_ptr<ASTNode> _elseBranch) {
			condition = _condition;
			thenBranch = _thenBranch;
			elseBranch = _elseBranch;
		}
		void accept(Visitor* vis) {
			vis->visitConditionalExpr(this);
		}
	};

	class BinaryExpr : public ASTNode {
	public:
		Token op;
		shared_ptr<ASTNode> left;
		shared_ptr<ASTNode> right;

		BinaryExpr(shared_ptr<ASTNode> _left, Token _op, shared_ptr<ASTNode> _right) {
			left = _left;
			op = _op;
			right = _right;
		}
		void accept(Visitor* vis) {
			vis->visitBinaryExpr(this);
		}
	};

	class UnaryExpr : public ASTNode {
	public:
		Token op;
		shared_ptr<ASTNode> right;
		bool isPrefix;

		UnaryExpr(Token _op, shared_ptr<ASTNode> _right, bool _isPrefix) {
			op = _op;
			right = _right;
			isPrefix = _isPrefix;
		}
		void accept(Visitor* vis) {
			vis->visitUnaryExpr(this);
		}
	};

	class ArrayLiteralExpr : public ASTNode {
	public:
		vector<shared_ptr<ASTNode>> members;

		ArrayLiteralExpr(vector<shared_ptr<ASTNode>>& _members) {
			members = _members;
		}
		void accept(Visitor* vis) {
			vis->visitArrayDeclExpr(this);
		}
	};

	class CallExpr : public ASTNode {
	public:
		shared_ptr<ASTNode> callee;
		vector<shared_ptr<ASTNode>> args;

		CallExpr(shared_ptr<ASTNode> _callee, vector<shared_ptr<ASTNode>>& _args) {
			callee = _callee;
			args = _args;
		}
		void accept(Visitor* vis) {
			vis->visitCallExpr(this);
		}
	};

	//getting values from compound types using '.' or '[]'
	class FieldAccessExpr : public ASTNode {
	public:
		shared_ptr<ASTNode> callee;
		Token accessor;
		shared_ptr<ASTNode> field;

		FieldAccessExpr(shared_ptr<ASTNode> _callee, Token _accessor, shared_ptr<ASTNode> _field) {
			callee = _callee;
			accessor = _accessor;
			field = _field;
		}
		void accept(Visitor* vis) {
			vis->visitFieldAccessExpr(this);
		}
	};

	class SuperExpr : public ASTNode {
	public:
		Token methodName;

		SuperExpr(Token _methodName) {
			methodName = _methodName;
		}
		void accept(Visitor* vis) {
			vis->visitSuperExpr(this);
		}
	};

	class GroupingExpr : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;

		GroupingExpr(shared_ptr<ASTNode> _expr) {
			expr = _expr;
		}
		void accept(Visitor* vis) {
			vis->visitGroupingExpr(this);
		}
	};

	class AwaitExpr : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;

		AwaitExpr(shared_ptr<ASTNode> _expr) {
			expr = _expr;
		}
		void accept(Visitor* vis) {
			vis->visitAwaitExpr(this);
		}
	};

	class LiteralExpr : public ASTNode {
	public:
		Token token;

		LiteralExpr(Token _token) {
			token = _token;
		}
		void accept(Visitor* vis) {
			vis->visitLiteralExpr(this);
		}
	};

	struct StructEntry {
		shared_ptr<ASTNode> expr;
		Token name;
		StructEntry(Token _name, shared_ptr<ASTNode> _expr) : expr(_expr), name(_name) {};
	};

	class StructLiteral : public ASTNode {
	public:
		vector<StructEntry> fields;

		StructLiteral(vector<StructEntry> _fields) {
			fields = _fields;
		}
		void accept(Visitor* vis) {
			vis->visitStructLiteralExpr(this);
		}
	};

	class FuncLiteral : public ASTNode {
	public:
		vector<Token> args;
		int arity;
		shared_ptr<ASTNode> body;

		FuncLiteral(vector<Token> _args, shared_ptr<ASTNode> _body) {
			args = _args;
			arity = _args.size();
			body = _body;
		}
		void accept(Visitor* vis) {
			vis->visitFuncLiteral(this);
		}
	};

	class ModuleAccessExpr : public ASTNode {
	public:
		Token moduleName;
		Token ident;

		ModuleAccessExpr(Token _moduleName, Token _ident) {
			moduleName = _moduleName;
			ident = _ident;
		}

		void accept(Visitor* vis) {
			vis->visitModuleAccessExpr(this);
		}
	};

#pragma endregion

	#pragma region Statements

	//temporary, will replace with a native function
	class PrintStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;

		PrintStmt(shared_ptr<ASTNode> _expr) {
			expr = _expr;
		}
		void accept(Visitor* vis) {
			vis->visitPrintStmt(this);
		}
	};

	class ExprStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;

		ExprStmt(shared_ptr<ASTNode> _expr) {
			expr = _expr;
		}
		void accept(Visitor* vis) {
			vis->visitExprStmt(this);
		}
	};

	class VarDecl : public ASTDecl {
	public:
		shared_ptr<ASTNode> value;
		Token name;

		VarDecl(Token _name, shared_ptr<ASTNode> _value) {
			name = _name;
			value = _value;
		}
		void accept(Visitor* vis) {
			vis->visitVarDecl(this);
		}
		Token getName() { return name; }
	};

	class BlockStmt : public ASTNode {
	public:
		vector<shared_ptr<ASTNode>> statements;

		BlockStmt(vector<shared_ptr<ASTNode>> _statements) {
			statements = _statements;
		}
		void accept(Visitor* vis) {
			vis->visitBlockStmt(this);
		}
	};

	class IfStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> thenBranch;
		shared_ptr<ASTNode> elseBranch;
		shared_ptr<ASTNode> condition;

		IfStmt(shared_ptr<ASTNode> _then, shared_ptr<ASTNode> _else, shared_ptr<ASTNode> _condition) {
			condition = _condition;
			thenBranch = _then;
			elseBranch = _else;
		}
		void accept(Visitor* vis) {
			vis->visitIfStmt(this);
		}
	};

	class WhileStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> body;
		shared_ptr<ASTNode> condition;

		WhileStmt(shared_ptr<ASTNode> _body, shared_ptr<ASTNode> _condition) {
			body = _body;
			condition = _condition;
		}
		void accept(Visitor* vis) {
			vis->visitWhileStmt(this);
		}
	};

	class ForStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> body;
		shared_ptr<ASTNode> init;
		shared_ptr<ASTNode> condition;
		shared_ptr<ASTNode> increment;

		ForStmt(shared_ptr<ASTNode> _init, shared_ptr<ASTNode> _condition, shared_ptr<ASTNode> _increment, shared_ptr<ASTNode> _body) {
			init = _init;
			condition = _condition;
			increment = _increment;
			body = _body;
		}
		void accept(Visitor* vis) {
			vis->visitForStmt(this);
		}
	};

	class BreakStmt : public ASTNode {
	public:
		Token token;

		BreakStmt(Token _token) {
			token = _token;
		}
		void accept(Visitor* vis) {
			vis->visitBreakStmt(this);
		}
	};

	class ContinueStmt : public ASTNode {
	public:
		Token token;

		ContinueStmt(Token _token) {
			token = _token;
		}
		void accept(Visitor* vis) {
			vis->visitContinueStmt(this);
		}
	};

	class SwitchStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;
		vector<shared_ptr<CaseStmt>> cases;
		bool hasDefault;

		SwitchStmt(shared_ptr<ASTNode> _expr, vector<shared_ptr<CaseStmt>> _cases, bool _hasDefault) {
			expr = _expr;
			cases = _cases;
			hasDefault = _hasDefault;
		}
		void accept(Visitor* vis) {
			vis->visitSwitchStmt(this);
		}
	};

	class CaseStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;
		vector<shared_ptr<ASTNode>> stmts;
		Token caseType;//case or default

		CaseStmt(shared_ptr<ASTNode> _expr, vector<shared_ptr<ASTNode>>& _stmts) {
			expr = _expr;
			stmts = _stmts;
		}
		void accept(Visitor* vis) {
			vis->visitCaseStmt(this);
		}
	};

	class FuncDecl : public ASTDecl {
	public:
		vector<Token> args;
		int arity;
		shared_ptr<ASTNode> body;
		Token name;

		FuncDecl(Token _name, vector<Token> _args, shared_ptr<ASTNode> _body) {
			name = _name;
			args = _args;
			arity = _args.size();
			body = _body;
		}
		void accept(Visitor* vis) {
			vis->visitFuncDecl(this);
		}
		Token getName() { return name; }
	};

	class ReturnStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;
		//for error reporting
		Token keyword;

		ReturnStmt(shared_ptr<ASTNode> _expr, Token _keyword) {
			expr = _expr;
			keyword = _keyword;
		}
		void accept(Visitor* vis) {
			vis->visitReturnStmt(this);
		}
	};

	class ClassDecl : public ASTDecl {
	public:
		Token name;
		Token inheritedClass;
		vector<shared_ptr<ASTNode>> methods;
		bool inherits;

		ClassDecl(Token _name, vector<shared_ptr<ASTNode>> _methods, Token _inheritedClass, bool _inherits) {
			name = _name;
			methods = _methods;
			inheritedClass = _inheritedClass;
			inherits = _inherits;
		}
		void accept(Visitor* vis) {
			vis->visitClassDecl(this);
		}
		Token getName() { return name; }
	};
#pragma endregion

}