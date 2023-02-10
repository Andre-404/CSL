#pragma once
#include "ASTDefs.h"
#include "parser.h"
#include "../Includes/fmt/format.h"
#include <map>
#include <queue>
#include <iterator>

#define MACRO_RECURSION_DEPTH 128

namespace AST {
    class Parser;

    enum class TransitionType {
        None,
        ConsumeToken,
        ConsumeExpr,
        ConsumeTT,
        LoopBegin,
        LoopIterate,
        LoopEnd
    };

    enum class LoopType {
        None,
        Star,
        Paren
    };

    struct Transition {
        int matcherPtr = 0, argsPtr = 0;
        TransitionType type = TransitionType::None;

        Transition() = default;
        Transition(int _matcherPtr, int _argsPtr, TransitionType _type) {
            matcherPtr = _matcherPtr;
            argsPtr = _argsPtr;
            type = _type;
        }
    };

    struct exprMetaVar {
        std::map<vector<int>, ASTNodePtr> vals;

        exprMetaVar() = default;
    };

    struct ttMetaVar {
        std::map<vector<int>, vector<Token>> vals;

        ttMetaVar() = default;
    };

    class MatchPattern {
    private:
        vector<Token> pattern;
        vector<int> loopJumps; // marks where to jump when encountering macro loops
        vector<LoopType> loopTypes; // marks start of loops with type of loop
        Parser* parser;

        void checkAndPrecalculatePattern();

    public:
        MatchPattern(vector<Token> _pattern, Parser* _parser) {
            pattern = _pattern;
            parser = _parser;
            loopTypes = vector<LoopType>(pattern.size(), LoopType::None);
            // Initially, for all x, loopJumps[x] == x
            for (int i = 0; i < pattern.size(); i++) {
                loopJumps.push_back(i);
            }
            checkAndPrecalculatePattern();
        }

        bool interpret(vector<Token>& args, unordered_map<string, std::unique_ptr<exprMetaVar>>& exprMetaVars, unordered_map<string, std::unique_ptr<ttMetaVar>>& ttMetaVars) const;
    };

    class Macro {
    private:
        Parser* parser;
        Token name;
    public:
        vector<MatchPattern> matchers;
        vector<vector<Token>> transcribers;

        Macro();
        Macro(Token _name, Parser* _parser);

        ASTNodePtr expand(vector<Token>& args, const Token& callerToken);
    };

    // Used for expanding macros in the AST
    class MacroExpander : public Visitor {
    private:
        int recursionDepth = 0;
        Parser* parser;
        ASTNodePtr expansion = nullptr;

    public:
        explicit MacroExpander(Parser* _parser);

        void expand(ASTNodePtr node);

        void visitAssignmentExpr(AssignmentExpr* expr) override;
        void visitSetExpr(SetExpr* expr) override;
        void visitConditionalExpr(ConditionalExpr* expr) override;
        void visitBinaryExpr(BinaryExpr* expr) override;
        void visitUnaryExpr(UnaryExpr* expr) override;
        void visitCallExpr(CallExpr* expr) override;
        void visitFieldAccessExpr(FieldAccessExpr* expr) override;
        void visitAsyncExpr(AsyncExpr* expr) override;
        void visitAwaitExpr(AwaitExpr* expr) override;
        void visitArrayLiteralExpr(ArrayLiteralExpr* expr) override;
        void visitStructLiteralExpr(StructLiteral* expr) override;
        void visitLiteralExpr(LiteralExpr* expr) override;
        void visitFuncLiteral(FuncLiteral* expr) override;
        void visitSuperExpr(SuperExpr* expr) override;
        void visitModuleAccessExpr(ModuleAccessExpr* expr) override;
        void visitMacroExpr(MacroExpr* expr) override;

        void visitVarDecl(VarDecl* decl) override;
        void visitFuncDecl(FuncDecl* decl) override;
        void visitClassDecl(ClassDecl* decl) override;

        void visitPrintStmt(PrintStmt* stmt) override;
        void visitExprStmt(ExprStmt* stmt) override;
        void visitBlockStmt(BlockStmt* stmt) override;
        void visitIfStmt(IfStmt* stmt) override;
        void visitWhileStmt(WhileStmt* stmt) override;
        void visitForStmt(ForStmt* stmt) override;
        void visitBreakStmt(BreakStmt* stmt) override;
        void visitContinueStmt(ContinueStmt* stmt) override;
        void visitSwitchStmt(SwitchStmt* stmt) override;
        void visitCaseStmt(CaseStmt* _case) override;
        void visitAdvanceStmt(AdvanceStmt* stmt) override;
        void visitReturnStmt(ReturnStmt* stmt) override;
    };
}