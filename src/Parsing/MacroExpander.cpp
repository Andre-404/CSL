#include "MacroExpander.h"



AST::MacroExpander::MacroExpander(Parser* _parser) {
    parser = _parser;
}

void AST::MacroExpander::expand(ASTNodePtr node) {
    if (!node) return;
    node->accept(this);
    if (expansion != nullptr) node = expansion;
    expansion = nullptr;
}

void AST::MacroExpander::visitAssignmentExpr(AssignmentExpr* expr) {
    expand(expr->value);
}
void AST::MacroExpander::visitSetExpr(SetExpr* expr) { expand(expr->value); }
void AST::MacroExpander::visitConditionalExpr(ConditionalExpr* expr) {
    expand(expr->condition);
    expand(expr->thenBranch);
    expand(expr->elseBranch);
}
void AST::MacroExpander::visitBinaryExpr(BinaryExpr* expr) {
    expand(expr->left);
    expand(expr->right);
}
void AST::MacroExpander::visitUnaryExpr(UnaryExpr* expr) {
    expand(expr->right);
}
void AST::MacroExpander::visitCallExpr(CallExpr* expr) {
    for (const auto& arg : expr->args) {
        expand(arg);
    }
}
void AST::MacroExpander::visitFieldAccessExpr(FieldAccessExpr* expr) {}
void AST::MacroExpander::visitAsyncExpr(AsyncExpr* expr) {
    for (auto& arg : expr->args) {
        expand(arg);
    }
}
void AST::MacroExpander::visitAwaitExpr(AwaitExpr* expr) { expand(expr->expr); }
void AST::MacroExpander::visitArrayLiteralExpr(ArrayLiteralExpr* expr) {
    for (const auto& member : expr->members) {
        expand(member);
    }
}
void AST::MacroExpander::visitStructLiteralExpr(StructLiteral* expr) {
    for (const StructEntry& entry : expr->fields) {
        expand(entry.expr);
    }
}
void AST::MacroExpander::visitLiteralExpr(LiteralExpr* expr) {}
void AST::MacroExpander::visitFuncLiteral(FuncLiteral* expr) {
    expand(expr->body);
}
void AST::MacroExpander::visitSuperExpr(SuperExpr* expr) {}
void AST::MacroExpander::visitModuleAccessExpr(ModuleAccessExpr* expr) {}

void AST::MacroExpander::visitMacroExpr(MacroExpr* expr) {
    recursionDepth++;
    if (recursionDepth >= MACRO_RECURSION_DEPTH) {
        throw parser->error(expr->macroName, fmt::format("Macro recursion depth({}) exceeded.", MACRO_RECURSION_DEPTH));
    }

    expansion = parser->macros[expr->macroName.getLexeme()]->expand(expr->args, expr->macroName);
    expand(expansion);

    recursionDepth--;
}

void AST::MacroExpander::visitVarDecl(VarDecl* decl) {
    expand(decl->value);
}

void AST::MacroExpander::visitFuncDecl(FuncDecl* decl) { expand(decl->body); }
void AST::MacroExpander::visitClassDecl(ClassDecl* decl) {
    for (const auto& method : decl->methods) {
        expand(method);
    }
}

void AST::MacroExpander::visitPrintStmt(PrintStmt* stmt) { expand(stmt->expr); }
void AST::MacroExpander::visitExprStmt(ExprStmt* stmt) { expand(stmt->expr); }
void AST::MacroExpander::visitBlockStmt(BlockStmt* stmt) {
    for (const auto& line : stmt->statements) {
        expand(line);
    }
}
void AST::MacroExpander::visitIfStmt(IfStmt* stmt) {
    expand(stmt->condition);
    expand(stmt->thenBranch);
    expand(stmt->elseBranch);
}
void AST::MacroExpander::visitWhileStmt(WhileStmt* stmt) {
    expand(stmt->condition);
    expand(stmt->body);
}
void AST::MacroExpander::visitForStmt(ForStmt* stmt) {
    expand(stmt->init);
    expand(stmt->condition);
    expand(stmt->increment);
    expand(stmt->body);
}
void AST::MacroExpander::visitBreakStmt(BreakStmt* stmt) {}
void AST::MacroExpander::visitContinueStmt(ContinueStmt* stmt) {}
void AST::MacroExpander::visitSwitchStmt(SwitchStmt* stmt) {
    expand(stmt->expr);
    for (const auto& _case : stmt->cases) {
        expand(_case);
    }
}
void AST::MacroExpander::visitCaseStmt(CaseStmt* stmt) {
    for (const auto& statement : stmt->stmts) {
        expand(statement);
    }
}
void AST::MacroExpander::visitAdvanceStmt(AdvanceStmt* stmt) {}
void AST::MacroExpander::visitReturnStmt(ReturnStmt* stmt) { expand(stmt->expr); }

AST::Macro::Macro() {
    parser = nullptr;
}

AST::Macro::Macro(Token _name, Parser* _parser) {
    name = _name;
    parser = _parser;
}

AST::ASTNodePtr AST::Macro::expand(vector<Token>& args, const Token& callerToken) {
    unordered_map<string, std::unique_ptr<exprMetaVar>> exprMetaVars;
    unordered_map<string, std::unique_ptr<ttMetaVar>> ttMetaVars;

    // Attempt to match every macro matcher to arguments ...
    for (const MatchPattern& matcher : matchers) {
        exprMetaVars.clear();
        ttMetaVars.clear();

        // Try to interpret arguments into meta variables
        if (!matcher.interpret(args, exprMetaVars, ttMetaVars)) { continue; }

        // Expand all loops and substitute all tt meta variables

        // Convert tokens to (partial) AST
        parser->parseMode = ParseMode::Macro;

        parser->parseMode = ParseMode::Standard;
        // Substitute all expr meta variables

        // Return AST
    }

    // ... we found no appropriate matcher
    parser->error(callerToken, "Couldn't find an appropriate matcher for the given macro arguments.");
    return nullptr;
}


bool AST::MatchPattern::interpret(vector<Token>& args, unordered_map<string, std::unique_ptr<exprMetaVar>>& exprMetaVars, unordered_map<string, std::unique_ptr<ttMetaVar>>& ttMetaVars) const {
    parser->currentContainer = &args;

    auto isAtEnd = [&](int i) { return i >= args.size(); };

    vector<vector<int>> dp(args.size() + 1, vector<int>(pattern.size() + 1));
    vector<vector<Transition>> backTransitions(args.size() + 1, vector<Transition>(pattern.size() + 1));
    dp[0][0] = 1;

    for (int i = 0; i < args.size(); i++) {
        std::queue<int> toProcess;
        for (int j = 0; j < pattern.size(); j++) {
            toProcess.push(j);
        }
        while (!toProcess.empty()) {
            int j = toProcess.front(); toProcess.pop();
            // We should only try and transition if this state can be reached unambiguously
            if (dp[i][j] != 1) continue;
            // Transitions
            // Ignore loop parens
            if (loopTypes[j] == LoopType::Paren) {
                dp[i + 1][j + 1] += dp[i][j];
                backTransitions[i + 1][j + 1] = Transition(i, j, TransitionType::None);
                continue;
            }

            // Regular token
            if (pattern[j].type != TokenType::DOLLAR && loopTypes[j] == LoopType::None) {
                if (pattern[j].type != args[i].type) continue;
                dp[i + 1][j + 1] += dp[i][j];
                backTransitions[i + 1][j + 1] = Transition(i, j, TransitionType::ConsumeToken);
            }
            // Looping expression or meta variable
            else {
                if (pattern[j].type == TokenType::DOLLAR) {
                    // Meta variable
                    if (pattern[j + 1].type == TokenType::IDENTIFIER) {
                        // Token tree
                        if (pattern[j + 3].type == TokenType::TT) {
                            parser->currentPtr = i;
                            try {
                                parser->readTokenTree();
                                dp[parser->currentPtr][j + 4] += dp[i][j];
                                backTransitions[parser->currentPtr][j + 4] = Transition(i, j, TransitionType::ConsumeTT);
                            }
                            catch (ParserException& e) { continue; }
                        }
                        // Expression
                        else {
                            parser->currentPtr = i;
                            try {
                                parser->expression();
                                dp[parser->currentPtr][j + 4] += dp[i][j];
                                backTransitions[parser->currentPtr][j + 4] = Transition(i, j, TransitionType::ConsumeExpr);
                            }
                            catch (ParserException& e) { continue; }
                        }
                    }
                    // Loop start
                    else {
                        // Start loop
                        dp[i][j + 1] += dp[i][j];
                        backTransitions[i][j + 1] = Transition(i, j, TransitionType::LoopBegin);
                        toProcess.push(j + 1);
                        // Ignore loop
                        dp[i][loopJumps[j] + 1] += dp[i][j];
                        backTransitions[i][loopJumps[j] + 1] = Transition(i, j, TransitionType::None);
                        toProcess.push(loopJumps[j] + 1);
                    }
                }
                // Loop end
                else {
                    // Repeat loop
                    dp[i][loopJumps[j] + 1] += dp[i][j];
                    backTransitions[i][loopJumps[j] + 1] = Transition(i, j, TransitionType::LoopIterate);
                    toProcess.push(loopJumps[j] + 1);
                    // Continue without repeating
                    dp[i][j + 1] += dp[i][j];
                    backTransitions[i][j + 1] = Transition(i, j, TransitionType::LoopEnd);
                }
            }
        }
    }
    // Impossible to interpret arguments with this matcher pattern
    if (dp[args.size()][pattern.size()] == 0) return false;

    // Multiple interpretations of arguments possible
    if (dp[args.size()][pattern.size()] > 1) {
        // TODO: Make this error better
        throw parser->error(pattern[0], "Ambiguous arguments to macro, multiple interpretations possible.", true);
        return false;
    }

    // There is exactly 1 interpretation of arguments possible.
    int i = args.size(), j = pattern.size();
    vector<Transition> path;
    while (i != 0 || j != 0) {
        Transition t = backTransitions[i][j];
        path.push_back(t);
        i = t.argsPtr;
        j = t.matcherPtr;
    }
    std::reverse(path.begin(), path.end());
    return true;
}

// Checks if matcher pattern contains properly written loops and meta variables.
// Loops are also precalculated here.
void AST::MatchPattern::checkAndPrecalculatePattern() {

    // Counts '(', '{' and '['
    vector<int> closerCounts(3);

    auto isAtEnd = [&](int i) {
        return i >= pattern.size();
    };

    auto check = [&](int i, TokenType type) {
        if (i < 0 || i >= pattern.size()) return false;
        return pattern[i].type == type;
    };

    // Maps closerCounts to indexes in matcher pattern
    std::map<vector<int>, int> loopEntries;

    for (int i = 0; i < pattern.size(); i++) {
        if (pattern[i].type == TokenType::DOLLAR) {
            // Meta variable
            if (check(i + 1, TokenType::IDENTIFIER)) {
                i += 2;
                if (!check(i, TokenType::COLON)) {
                    throw parser->error(pattern[i], "Expected ':' after meta variable.");
                    continue;
                }
                i++;
                if (!check(i, TokenType::EXPR) && !check(i, TokenType::TT)) {
                    throw parser->error(pattern[i], "Expected 'expr' or 'tt' type fragments for meta variable.");
                    continue;
                }
                continue;
            }
            // Loop
            if (check(i + 1, TokenType::LEFT_PAREN)) {
                loopEntries[closerCounts] = i;
                loopTypes[i] = LoopType::Paren;
                closerCounts[0]++;
                i++;
                continue;
            }
            throw parser->error(pattern[i], "Expected '(' or identifier following '$'.");
        }
        else {
            if (check(i, TokenType::LEFT_PAREN)) closerCounts[0]++;
            if (check(i, TokenType::LEFT_BRACE)) closerCounts[1]++;
            if (check(i, TokenType::LEFT_BRACKET)) closerCounts[2]++;
            if (check(i, TokenType::RIGHT_PAREN)) {
                closerCounts[0]--;
                // This ')' finishes a loop
                if (loopEntries.contains(closerCounts)) {
                    loopTypes[i] = LoopType::Paren;
                    int entryPoint = loopEntries[closerCounts];
                    loopEntries.erase(closerCounts);

                    i++;
                    if (isAtEnd(i)) throw parser->error(pattern.back(), "Expected '*' or delimiter after macro loop");

                    // Delimiter
                    if (check(i, TokenType::COMMA) || check(i, TokenType::ARROW) || check(i, TokenType::DOT)) { i++; }

                    if (isAtEnd(i)) throw parser->error(pattern.back(), "Expected '*' after macro loop");

                    if (check(i, TokenType::STAR)) {
                        loopTypes[entryPoint] = LoopType::Star;
                        loopTypes[i] = LoopType::Star;
                        loopJumps[i] = entryPoint;
                        loopJumps[entryPoint] = i;
                    }
                    else {
                        throw parser->error(pattern.back(), "Expected '*', '+', '?' after macro loop");
                    }
                }
            }
            if (check(i, TokenType::RIGHT_BRACE)) closerCounts[1]--;
            if (check(i, TokenType::RIGHT_BRACKET)) closerCounts[2]--;
        }
    }
}
