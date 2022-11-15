#include "preprocessor.h"
#include <filesystem>
#include "../files.h"
#include <iostream>

using std::unordered_map;
using namespace preprocessing;

Preprocessor::Preprocessor(ErrorHandler& _handler) : errorHandler(_handler) {
	projectRootPath = "";
	hadError = false;
	curUnit = nullptr;
}

Preprocessor::~Preprocessor() {
	//for (preprocessUnit* pUnit : sortedUnits) delete pUnit;
}

bool Preprocessor::preprocessProject(string mainFilePath) {
	using namespace std::filesystem;
	path p(mainFilePath);

	//checks file validity
	if (p.extension().string() != ".csl" || p.stem().string() != "main" || !exists(p)) {
		errorHandler.addError(SystemError("Couldn't find main.csl"));
		return true;
	}

	projectRootPath = p.parent_path().string() + "/";
	CSLModule* mainModule = scanFile("main.csl");
	topsort(mainModule);
	return hadError;
}

CSLModule* Preprocessor::scanFile(string moduleName) {
	string fullPath = projectRootPath + moduleName;
	vector<Token> tokens = scanner.tokenizeSource(readFile(fullPath), moduleName);
	CSLModule* unit = new CSLModule(tokens, scanner.getFile());
	allUnits[moduleName] = unit;
	curUnit = unit;
	//detects macro and import, gets rid of newline tokens, and returns the tokens of dep names
	std::tuple<vector<Token>, unordered_map<string, Macro>> result = processDirectives(unit);
	vector<Macro> stack;
	replaceMacros(unit->tokens, std::get<1>(result), stack);
	vector<Token> depsToParse = std::get<0>(result);
	for (Token dep : depsToParse) {
		//since import names are strings, we get rid of ""
		string depName = dep.getLexeme();
		depName.erase(0, 1);
		depName.erase(depName.size() - 1, depName.size());
		//if we have already scanned module with name 'dep' we add it to the deps list of this module
		//to topsort it later
		if (allUnits.count(depName) > 0) {
			//if we detect a cyclical import we still continue parsing other files to detect as many errors as possible
			if (!allUnits[depName]->resolvedDeps) {
				error(dep, "Cyclical importing detected.");
				continue;
			}
			unit->deps.push_back(allUnits[depName]);
			continue;
		}
		std::filesystem::path p(projectRootPath + depName);
		if (std::filesystem::exists(p)) {
			unit->deps.push_back(scanFile(depName));
		}
		else {
			error(dep, "File " + depName + " doesn't exist.");
		}
	}
	unit->resolvedDeps = true;
	for (Token token : unit->tokens) {
		std::cout << token.getLexeme() << "\n";
	}
	return unit;
}


TokenType checkNext(vector<Token>& tokens, int pos) {
	if (pos + 1 >= tokens.size()) return TokenType::TOKEN_EOF;
	return tokens[pos + 1].type;
}

bool isAtEnd(vector<Token>& tokens, int pos) {
	return checkNext(tokens, pos) == TokenType::TOKEN_EOF;
}

bool containsMacro(vector<Macro>& macros, Macro& _macro) {
	for (int i = 0; i < macros.size(); i++) {
		if (macros[i].name.getLexeme() == _macro.name.getLexeme()) return true;
	}
	return false;
}

void Preprocessor::topsort(CSLModule* unit) {
	//TODO: we can get a stack overflow if this goes into deep recursion, try making a iterative stack based DFS implementation
	unit->traversed = true;
	for (CSLModule* dep : unit->deps) {
		if (!dep->traversed) topsort(dep);
	}
	sortedUnits.push_back(unit);
}

std::tuple<vector<Token>, unordered_map<string, Macro>> Preprocessor::processDirectives(CSLModule* unit) {
	vector<Token>& tokens = unit->tokens;
	vector<Token> importTokens;
	unordered_map<string, Macro> declaredMacros;
	for (int i = 0; i < tokens.size(); i++) {
		Token& token = tokens[i];
		if (token.type == TokenType::MACRO) {
			if (checkNext(tokens, i) != TokenType::IDENTIFIER) {
				error(tokens[i], "Expected macro name");
				continue;
			}
			//deletes the macro keyword and macro name
			//doing tokens.begin() twice because itertor might update
			tokens.erase(tokens.begin() + i);
			Token name = tokens[i];
			tokens.erase(tokens.begin() + i--);

			Macro curMacro = Macro(name);
			if (declaredMacros.count(name.str.getStr()) > 0) {
				error(tokens[i], "Macro redefinition not allowed.");
				continue;
			}
			//detecting if we have a function like macro(eg. macro add(a, b): (a + b)
			if (checkNext(tokens, i) == TokenType::LEFT_PAREN) {
				//used to later delete params and macro definition
				int curPos = ++i;
				//parses arguments
				while (!isAtEnd(tokens, i) && checkNext(tokens, i) != TokenType::RIGHT_PAREN) {
					Token& arg = tokens[++i];
					//checking if we already have a param of the same name
					for (int j = 0; j < curMacro.params.size(); j++) {
						if (arg.getLexeme() == curMacro.params[j].getLexeme()) {
							error(arg, "Cannot have 2 or more arguments of the same name.");
							break;
						}
					}
					curMacro.params.push_back(arg);
					if (checkNext(tokens, i) == TokenType::COMMA) ++i;
					else {
						error(tokens[i + 1], "Expected ',' after paramter name");
						break;
					}
				}
				//we continue to next loop iter here because if we're at EOF and try erasing i + 1 we'll get a error
				if (isAtEnd(tokens, i) || checkNext(tokens, i) != TokenType::RIGHT_PAREN) {
					error(tokens[i], "Expected ')' after arguments.");
					continue;
				}
				i++;
				//macro add(a, b): (a + b)
				//deletes  ^^^^^^
				auto it = tokens.begin();
				tokens.erase(it + curPos, it + 1 + i);
				i = curPos - 1;
				curMacro.isFunctionLike = true;
			}
			//every macro declaration must have ':' before it's body
			//(this is used to differentiate between normal macros and function like macros
			if (checkNext(tokens, i) != TokenType::COLON) {
				error(tokens[i], "Expected ':' following macro name.");
				continue;
			}
			tokens.erase(tokens.begin() + 1 + i);
			//loops until we hit a \n
			//for each iteration, we add the token to macro body and then delete it(keeping i in check so that we don't go outside of boundries)
			while (!isAtEnd(tokens, i) && checkNext(tokens, i) != TokenType::NEWLINE) {
				curMacro.value.push_back(tokens[++i]);
				tokens.erase(tokens.begin() + i--);
			}
			declaredMacros[curMacro.name.getLexeme()] = curMacro;
		}
		else if (token.type == TokenType::IMPORT) {
			if (checkNext(tokens, i) != TokenType::STRING) error(tokens[i], "Expected a module name.");
			//deletes both the import keyword and module name tokens from the token list, and add them to deps that need parsing
			tokens.erase(tokens.begin() + i);
			Token name = tokens[i];
			tokens.erase(tokens.begin() + i--);
			importTokens.push_back(name);
		}
		else if (token.type == TokenType::NEWLINE) {
			//this token is used for macro bodies, and isn't needed for anything else, so we delete it when we find it
			tokens.erase(tokens.begin() + i--);
		}
	}
	return std::make_tuple(importTokens, declaredMacros);
}

void Preprocessor::replaceMacros(vector<Token>& tokens, unordered_map<string, Macro>& macros, vector<Macro>& macroStack) {
	for (int i = 0; i < tokens.size(); i++) {
		Token token = tokens[i];
		if (token.type != TokenType::IDENTIFIER) continue;
		if (macros.count(token.getLexeme()) == 0) continue;
		Macro& macro = macros[token.getLexeme()];

		//if a macro is function like we need to parse it's arguments 
		//and replace the params inside the macro body with the provided args
		//this is a simple token replacement
		if (macro.isFunctionLike) {
			//delete macro name
			tokens.erase(tokens.begin() + i);
			//calling a function here to detect possible cyclical macros
			macroStack.push_back(macro);
			vector<Token> expanded = expandMacro(macro, tokens, i - 1, macros, macroStack);
			macroStack.pop_back();
			//for better error detection
			for (Token& expandedToken : expanded) {
				expandedToken.line = token.line;
				expandedToken.partOfMacro = true;
				expandedToken.macro = token.str;
			}
			//insert the macro body with the params replaced by args
			tokens.insert(tokens.begin() + i, expanded.begin(), expanded.end());
			i--;
			continue;
		}
		//normal macros
		//get rid of macro name
		tokens.erase(tokens.begin() + i);
		macroStack.push_back(macro);
		vector<Token> expanded = expandMacro(macro, macros, macroStack);
		for (Token& expandedToken : expanded) {
			expandedToken.line = token.line;
			expandedToken.partOfMacro = true;
			expandedToken.macro = token.str;
		}
		macroStack.pop_back();
		tokens.insert(tokens.begin() + i, expanded.begin(), expanded.end());
		i--;
	}
}

vector<Token> replaceFuncMacroParams(Macro& toExpand, vector<vector<Token>>& args) {
	//given a set of args, it replaces all params in the macro body with the args
	//this relies on args and params being in same positions in the array

	//copy macro body
	vector<Token> fin = toExpand.value;
	for (int i = 0; i < fin.size(); i++) {
		Token& token = fin[i];
		if (token.type == TokenType::IDENTIFIER) {
			for (int j = 0; j < toExpand.params.size(); j++) {
				//if this is a param, delete it and copy the args body
				if (token.getLexeme() == toExpand.params[j].getLexeme()) {
					fin.erase(fin.begin() + i);
					fin.insert(fin.begin() + i, args[j].begin(), args[j].end());
					i--;
				}
			}
		}
	}
	return fin;
}

int parseMacroFuncArgs(vector<vector<Token>>& args, vector<Token>& tokens, int pos) {
	int argCount = 0;
	//very scuffed solution, but it works for when a function call is passed as a argument
	//it works by keeping a "parenthesis" count, and we only exit if we encounter a right paren and
	//parenCount is 0(meaning we're not inside some other function call)
	int parenCount = 0;
	args.push_back(vector<Token>());
	while (!isAtEnd(tokens, pos) && checkNext(tokens, pos) != TokenType::RIGHT_PAREN) {
		//since arguments can be a undefined number of tokens long, we need to parse until we hit either ',' or ')'
		while (!isAtEnd(tokens, pos) && (parenCount > 0 || (checkNext(tokens, pos) != TokenType::COMMA && checkNext(tokens, pos) != TokenType::RIGHT_PAREN))) {
			//if parenCount > 0 then we're inside either a call or something else
			if (checkNext(tokens, pos) == TokenType::LEFT_PAREN) parenCount++;
			else if (checkNext(tokens, pos) == TokenType::RIGHT_PAREN) parenCount--;
			args[argCount].push_back(tokens[++pos]);
		}
		//arg delimiter
		if (checkNext(tokens, pos) == TokenType::COMMA) pos++;
		argCount++;
		args.push_back(vector<Token>());
	} 
	return pos;
}

vector<Token> Preprocessor::expandMacro(Macro& toExpand, unordered_map<string, Macro>& macros, vector<Macro>& macroStack) {
	//copy macro body
	vector<Token> tokens = toExpand.value;
	replaceMacros(tokens, macros, macroStack);
	/*
	for (int i = 0; i < tokens.size(); i++) {
		Token& token = tokens[i];
		if (token.type != TokenType::IDENTIFIER) continue;
		if (macros.count(token.getLexeme()) == 0) continue;

		//if the macro is already on the stack, it's a recursive macro and we throw an error
		Macro& macro = macros[token.getLexeme()];
		if (containsMacro(macroStack, macro)) {
			error(macro.name, "Recursive macro expansion detected.");
			tokens.clear();
			continue;
		}

		macroStack.push_back(macro);
		if (macro.isFunctionLike) {
			//get rid of macro name
			tokens.erase(tokens.begin() + i);
			//recursive expansion
			//i - 1 because when we delete the name(if the macro is called correctly) we'll be right at the '(', and since we're
			//always checking ahead, and not at the current position, we need to backtrack a little
			vector<Token> expanded = expandMacro(macro, tokens, i - 1, macros, macroStack);
			//inserts expanded body
			tokens.insert(tokens.begin() + i, expanded.begin(), expanded.end());
			i--;
			macroStack.pop_back();
			continue;
		}
		//recursivly expands the macro
		vector<Token> expanded = expandMacro(macro, macros, macroStack);
		//delete the name
		tokens.erase(tokens.begin() + i);
		//inserts expanded body
		tokens.insert(tokens.begin() + i, expanded.begin(), expanded.end());
		i--;
		//pops the macro used for his expansion(to preserve the stack order)
		macroStack.pop_back();
	}*/
	return tokens;
}

//replaces the params inside the macro body with the args provided
vector<Token> Preprocessor::expandMacro(Macro& toExpand, vector<Token>& callTokens, int pos, unordered_map<string, Macro>& macros, vector<Macro>& macroStack) {
	if (checkNext(callTokens, pos) != TokenType::LEFT_PAREN) {
		error(callTokens[pos], "Expected a '(' for macro arguments.");
		return vector<Token>();
	}
	pos++;
	//parses the args
	vector<vector<Token>> args;
	int newPos = parseMacroFuncArgs(args, callTokens, pos);
	if (checkNext(callTokens, newPos) != TokenType::RIGHT_PAREN) {
		error(callTokens[newPos], "Expected a ')' after macro args.");
		return vector<Token>();
	}
	newPos++;
	//.... macro(arg1, arg2)
	//erases:   ^^^^^^^^^^^^
	callTokens.erase(callTokens.begin() + pos, callTokens.begin() + newPos + 1);
	//replaces the params in the macro body with args we've parsed
	vector<Token> tokens = replaceFuncMacroParams(toExpand, args);

	replaceMacros(tokens, macros, macroStack);
	/*
	//checks for macros in the new macro body
	for (int i = 0; i < tokens.size(); i++) {
		Token& token = tokens[i];
		if (token.type != TokenType::IDENTIFIER) continue;
		if (macros.count(token.getLexeme()) == 0) continue;

		Macro& macro = macros[token.getLexeme()];
		if (containsMacro(macroStack, macro)) {
			error(macro.name, "Recursive macro expansion detected.");
			tokens.clear();
			break;
		}
		macroStack.push_back(macro);

		if (macro.isFunctionLike) {
			//erases the name
			tokens.erase(tokens.begin() + i);
			//i - 1 because when we delete the name(if the macro is called correctly) we'll be right at the '(', and since we're
			//always checking ahead, and not at the current position, we need to backtrack a little
			vector<Token> expanded = expandMacro(_macro, tokens, i - 1);
			tokens.insert(tokens.begin() + i, expanded.begin(), expanded.end());
			i--;
			//preserves stack behaviour
			macroStack.pop_back();
			continue;
		}
		//recursive expansion
		vector<Token> expanded = expandMacro(_macro);
		//erases the name
		tokens.erase(tokens.begin() + i);
		tokens.insert(tokens.begin() + i, expanded.begin(), expanded.end());
		i--;
		//preserves stack behaviour
		macroStack.pop_back();
	}*/
	return tokens;
}

void Preprocessor::error(Token token, string msg) {
	hadError = true;
	errorHandler.addError(CompileTimeError(msg, curUnit, token));
}