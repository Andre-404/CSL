#pragma once
#include "ASTDefs.h"

namespace AST {
	//used for debugging, controlled with AST_DEBUG in common.h
	class ASTPrinter : public visitor {
		void visitAssignmentExpr(AssignmentExpr* expr);
		void visitSetExpr(SetExpr* expr);
		void visitConditionalExpr(ConditionalExpr* expr);
		void visitBinaryExpr(BinaryExpr* expr);
		void visitUnaryExpr(UnaryExpr* expr);
		void visitCallExpr(CallExpr* expr);
		void visitFieldAccessExpr(FieldAccessExpr* expr);
		void visitGroupingExpr(GroupingExpr* expr);
		void visitArrayDeclExpr(ArrayLiteralExpr* expr);
		void visitStructLiteralExpr(StructLiteral* expr);
		void visitLiteralExpr(LiteralExpr* expr);
		void visitSuperExpr(SuperExpr* expr);

		void visitVarDecl(VarDecl* decl);
		void visitFuncDecl(FuncDecl* decl);
		void visitClassDecl(ClassDecl* decl);

		void visitPrintStmt(PrintStmt* stmt);
		void visitExprStmt(ExprStmt* stmt);
		void visitBlockStmt(BlockStmt* stmt);
		void visitIfStmt(IfStmt* stmt);
		void visitWhileStmt(WhileStmt* stmt);
		void visitForStmt(ForStmt* stmt);
		void visitBreakStmt(BreakStmt* stmt);
		void visitContinueStmt(ContinueStmt* stmt);
		void visitSwitchStmt(SwitchStmt* stmt);
		void visitCaseStmt(CaseStmt* _case);
		void visitReturnStmt(ReturnStmt* stmt);
	};
}