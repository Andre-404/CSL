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
	class StructLiteral;
	class LiteralExpr;
	class SuperExpr;

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
	class visitor {
	public:
		virtual void visitAssignmentExpr(AssignmentExpr* expr) = 0;
		virtual void visitSetExpr(SetExpr* expr) = 0;
		virtual void visitConditionalExpr(ConditionalExpr* expr) = 0;
		virtual void visitBinaryExpr(BinaryExpr* expr) = 0;
		virtual void visitUnaryExpr(UnaryExpr* expr) = 0;
		virtual void visitCallExpr(CallExpr* expr) = 0;
		virtual void visitFieldAccessExpr(FieldAccessExpr* expr) = 0;
		virtual void visitGroupingExpr(GroupingExpr* expr) = 0;
		virtual void visitArrayDeclExpr(ArrayLiteralExpr* expr) = 0;
		virtual void visitStructLiteralExpr(StructLiteral* expr) = 0;
		virtual void visitLiteralExpr(LiteralExpr* expr) = 0;
		virtual void visitSuperExpr(SuperExpr* expr) = 0;

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
		virtual void accept(visitor* vis) = 0;
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
		void accept(visitor* vis) {
			vis->visitAssignmentExpr(this);
		}
	};

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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
			vis->visitUnaryExpr(this);
		}
	};

	class ArrayLiteralExpr : public ASTNode {
	public:
		vector<shared_ptr<ASTNode>> members;

		ArrayLiteralExpr(vector<shared_ptr<ASTNode>>& _members) {
			members = _members;
		}
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
			vis->visitCallExpr(this);
		}
	};

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
		void accept(visitor* vis) {
			vis->visitFieldAccessExpr(this);
		}
	};

	class SuperExpr : public ASTNode {
	public:
		Token methodName;

		SuperExpr(Token _methodName) {
			methodName = _methodName;
		}
		void accept(visitor* vis) {
			vis->visitSuperExpr(this);
		}
	};

	class GroupingExpr : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;

		GroupingExpr(shared_ptr<ASTNode> _expr) {
			expr = _expr;
		}
		void accept(visitor* vis) {
			vis->visitGroupingExpr(this);
		}
	};

	class LiteralExpr : public ASTNode {
	public:
		Token token;

		LiteralExpr(Token _token) {
			token = _token;
		}
		void accept(visitor* vis) {
			vis->visitLiteralExpr(this);
		}
	};

	struct structEntry {
		shared_ptr<ASTNode> expr;
		Token name;
		structEntry(Token _name, shared_ptr<ASTNode> _expr) : expr(_expr), name(_name) {};
	};

	class StructLiteral : public ASTNode {
	public:
		vector<structEntry> fields;

		StructLiteral(vector<structEntry> _fields) {
			fields = _fields;
		}
		void accept(visitor* vis) {
			vis->visitStructLiteralExpr(this);
		}
	};

#pragma endregion

#pragma region Statements

	class PrintStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;

		PrintStmt(shared_ptr<ASTNode> _expr) {
			expr = _expr;
		}
		void accept(visitor* vis) {
			vis->visitPrintStmt(this);
		}
	};

	class ExprStmt : public ASTNode {
	public:
		shared_ptr<ASTNode> expr;

		ExprStmt(shared_ptr<ASTNode> _expr) {
			expr = _expr;
		}
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
			vis->visitForStmt(this);
		}
	};

	class BreakStmt : public ASTNode {
	public:
		Token token;

		BreakStmt(Token _token) {
			token = _token;
		}
		void accept(visitor* vis) {
			vis->visitBreakStmt(this);
		}
	};

	class ContinueStmt : public ASTNode {
	public:
		Token token;

		ContinueStmt(Token _token) {
			token = _token;
		}
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
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
		void accept(visitor* vis) {
			vis->visitClassDecl(this);
		}
		Token getName() { return name; }
	};
#pragma endregion

}