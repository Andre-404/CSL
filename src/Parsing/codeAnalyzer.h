#pragma once
#include "ASTDefs.h"
#include <array>

namespace CodeAnalysis {
	struct FuncInfo {
		//for closures
		FuncInfo* enclosing;
		//locals
		std::array<AST::Variable&, 256> locals;
		uInt localCount;
		uInt scopeDepth;
		vector<int> scopeWithLoop;
		vector<int> scopeWithSwitch;
		FuncInfo(FuncInfo* _enclosing, bool isMethod);
	};

	class ASTAnalyzer : public AST::Visitor {
	public:
		void visitAssignmentExpr(AST::AssignmentExpr* expr);
		void visitSetExpr(AST::SetExpr* expr);
		void visitConditionalExpr(AST::ConditionalExpr* expr);
		void visitBinaryExpr(AST::BinaryExpr* expr);
		void visitUnaryExpr(AST::UnaryExpr* expr);
		void visitCallExpr(AST::CallExpr* expr);
		void visitFieldAccessExpr(AST::FieldAccessExpr* expr);
		void visitGroupingExpr(AST::GroupingExpr* expr);
		void visitAwaitExpr(AST::AwaitExpr* expr);
		void visitAsyncExpr(AST::AsyncExpr* expr);
		void visitArrayLiteralExpr(AST::ArrayLiteralExpr* expr);
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
	private:
		FuncInfo* current;
	};

}