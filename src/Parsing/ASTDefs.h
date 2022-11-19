#pragma once
#pragma once
#include "../modulesDefs.h"

namespace AST {

	class AssignmentExpr;
	class SetExpr;
	class ConditionalExpr;
	class BinaryExpr;
	class UnaryExpr;
	class ArrayLiteralExpr;
	class CallExpr;
	class FieldAccessExpr;
	class GroupingExpr;
	class UnaryVarAlterExpr;
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
		virtual void visitUnaryVarAlterExpr(UnaryVarAlterExpr* expr) = 0;
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
		ASTNode* value;

		AssignmentExpr(Token _name, ASTNode* _value) {
			name = _name;
			value = _value;
		}
		void accept(visitor* vis) {
			vis->visitAssignmentExpr(this);
		}
	};

	class SetExpr : public ASTNode {
	public:
		ASTNode* callee;
		ASTNode* field;
		Token accessor;
		Token op;
		ASTNode* value;

		SetExpr(ASTNode* _callee, ASTNode* _field, Token _accessor, Token _op, ASTNode* _val) {
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
		ASTNode* condition;
		ASTNode* thenBranch;
		ASTNode* elseBranch;

		ConditionalExpr(ASTNode* _condition, ASTNode* _thenBranch, ASTNode* _elseBranch) {
			condition = condition;
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
		ASTNode* left;
		ASTNode* right;

		BinaryExpr(ASTNode* _left, Token _op, ASTNode* _right) {
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
		ASTNode* right;
		bool isPrefix;

		UnaryExpr(Token _op, ASTNode* _right, bool _isPrefix) {
			op = op;
			right = right;
			isPrefix = _isPrefix;
		}
		void accept(visitor* vis) {
			vis->visitUnaryExpr(this);
		}
	};

	class ArrayLiteralExpr : public ASTNode {
	public:
		vector<ASTNode*> members;

		ArrayLiteralExpr(vector<ASTNode*>& _members) {
			members = _members;
		}
		void accept(visitor* vis) {
			vis->visitArrayDeclExpr(this);
		}
	};

	class CallExpr : public ASTNode {
	public:
		ASTNode* callee;
		vector<ASTNode*> args;

		CallExpr(ASTNode* _callee, vector<ASTNode*>& _args) {
			callee = _callee;
			args = _args;
		}
		void accept(visitor* vis) {
			vis->visitCallExpr(this);
		}
	};

	class FieldAccessExpr : public ASTNode {
	public:
		ASTNode* callee;
		Token accessor;
		ASTNode* field;

		FieldAccessExpr(ASTNode* _callee, Token _accessor, ASTNode* _field) {
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
		ASTNode* expr;

		GroupingExpr(ASTNode* _expr) {
			expr = _expr;
		}
		void accept(visitor* vis) {
			vis->visitGroupingExpr(this);
		}
	};

	class UnaryVarAlterExpr : public ASTNode {
	public:
		ASTNode* incrementExpr;
		Token token;
		bool isPrefix;

		UnaryVarAlterExpr(ASTNode* _incrementExpr, Token _token, bool _isPrefix) {
			incrementExpr = _incrementExpr;
			token = _token;
			isPrefix = _isPrefix;
		}
		void accept(visitor* vis) {
			vis->visitUnaryVarAlterExpr(this);
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
		ASTNode* expr;
		Token name;
		structEntry(Token _name, ASTNode* _expr) : expr(_expr), name(_name) {};
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
		ASTNode* expr;

		PrintStmt(ASTNode* _expr) {
			expr = _expr;
		}
		void accept(visitor* vis) {
			vis->visitPrintStmt(this);
		}
	};

	class ExprStmt : public ASTNode {
	public:
		ASTNode* expr;

		ExprStmt(ASTNode* _expr) {
			expr = _expr;
		}
		void accept(visitor* vis) {
			vis->visitExprStmt(this);
		}
	};

	class VarDecl : public ASTDecl {
	public:
		ASTNode* value;
		Token name;

		VarDecl(Token _name, ASTNode* _value) {
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
		vector<ASTNode*> statements;

		BlockStmt(vector<ASTNode*> _statements) {
			statements = _statements;
		}
		void accept(visitor* vis) {
			vis->visitBlockStmt(this);
		}
	};

	class IfStmt : public ASTNode {
	public:
		ASTNode* thenBranch;
		ASTNode* elseBranch;
		ASTNode* condition;

		IfStmt(ASTNode* _then, ASTNode* _else, ASTNode* _condition) {
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
		ASTNode* body;
		ASTNode* condition;

		WhileStmt(ASTNode* _body, ASTNode* _condition) {
			body = _body;
			condition = _condition;
		}
		void accept(visitor* vis) {
			vis->visitWhileStmt(this);
		}
	};

	class ForStmt : public ASTNode {
	public:
		ASTNode* body;
		ASTNode* init;
		ASTNode* condition;
		ASTNode* increment;

		ForStmt(ASTNode* _init, ASTNode* _condition, ASTNode* _increment, ASTNode* _body) {
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
		ASTNode* expr;
		vector<ASTNode*> cases;
		bool hasDefault;

		SwitchStmt(ASTNode* _expr, vector<ASTNode*> _cases, bool _hasDefault) {
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
		ASTNode* expr;
		vector<ASTNode*> stmts;
		Token caseType;//case or default

		CaseStmt(ASTNode* _expr, vector<ASTNode*>& _stmts) {
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
		ASTNode* body;
		Token name;

		FuncDecl(Token _name, vector<Token> _args, ASTNode* _body) {
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
		ASTNode* expr;
		//for error reporting
		Token keyword;

		ReturnStmt(ASTNode* _expr, Token _keyword) {
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
		vector<ASTNode*> methods;
		bool inherits;

		ClassDecl(Token _name, vector<ASTNode*> _methods, Token _inheritedClass, bool _inherits) {
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