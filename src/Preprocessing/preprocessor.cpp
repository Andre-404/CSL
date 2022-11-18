#include "preprocessor.h"
#include <filesystem>
#include "../files.h"
#include <iostream>


using std::unordered_set;
using std::unordered_map;
using namespace preprocessing;


// Helper functions for preprocessor and macros

// Securely retrieve type of tokens[pos]
TokenType getTypeAt(const vector<Token>& tokens, const int pos) {
	if (pos < 0 || pos >= tokens.size()) return TokenType::TOKEN_EOF;
	return tokens[pos].type;
}

// Gets arguments of form (x, y, z...) and move given index to end of arguments
vector<vector<Token>> getArguments(const vector<Token>& value, int& i) {
	int bracketBalance = 0;
	vector<vector<Token>> args = { {} };

	auto getTokenType = [&](int i) {
		if (i < 0 || value.size() <= i) return TokenType::TOKEN_EOF;
		return value[i].type;
	};

	do {
		switch (getTokenType(i)) {
		case TokenType::LEFT_PAREN:
			if (bracketBalance > 0) args.back().push_back(value[i]);
			bracketBalance++;
			break;
		case TokenType::RIGHT_PAREN:
			bracketBalance--;
			if (bracketBalance > 0) args.back().push_back(value[i]);
			break;
		case TokenType::COMMA:
			// Check if comma is splitting macro arguments
			if (bracketBalance == 1) { args.push_back(vector<Token>()); }
			else args.back().push_back(value[i]);
			break;
		case TokenType::WHITESPACE:
			break;
		default:
			args.back().push_back(value[i]);
			break;
		}
		i++;
	} while (getTokenType(i) != TokenType::TOKEN_EOF && getTokenType(i) != TokenType::NEWLINE && bracketBalance > 0);
	// Make sure i is at the ending bracket
	i--;

	if (bracketBalance != 0) {
		// Throw "we didn't find a regular bracket sequence for this macros arguments" error
	}

	return args;
}

// Takes a macro that starts at position source[i], fully expands it and appends it to destination, whilst moving i to the end of the macro
// Ignores elements of ignoredMacros (to prevent infinite recursion)
void processMacro(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros, vector<Token>& destination, const vector<Token>& source, int& i) {
	string macroName = source[i].getLexeme();

	if (getTypeAt(source, i + 1) == TokenType::WHITESPACE) i++;
	vector<Token> expanded;

	// Function-like macro
	if (getTypeAt(source, i + 1) == TokenType::LEFT_PAREN) {
		i++;
		vector<vector<Token>> args = getArguments(source, i);
		expanded = macros[macroName]->expand(macros, args, ignoredMacros);
	}
	// Object-like macro
	else {
		expanded = macros[macroName]->expand(macros, ignoredMacros);
	}

	destination.insert(destination.end(), expanded.begin(), expanded.end());
}

// Fully macro expands given tokenized expression
vector<Token> macroExpandExpression(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros, vector<Token>& expression) {
	vector<Token> result;
	for (int i = 0; i < (int)expression.size(); i++) {
		Token& token = expression[i];
		if (token.type != TokenType::IDENTIFIER) { result.push_back(token); continue; }
		string lexeme = token.getLexeme();
		// Expand found macros (ignoring already expanded macros to prevent infinite expansion)
		if (macros.contains(lexeme) && !ignoredMacros.contains(lexeme)) {
			processMacro(macros, ignoredMacros, result, expression, i);
		}
		else {
			result.push_back(token);
		}
	}
	return result;
}

ObjectMacro::ObjectMacro(Token _name) { name = _name; }

vector<Token> ObjectMacro::expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros) {
	ignoredMacros.insert(name.getLexeme());

	vector<Token> expanded = macroExpandExpression(macros, ignoredMacros, value);

	ignoredMacros.erase(name.getLexeme());
	
	return expanded;
}

vector<Token> ObjectMacro::expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& args, unordered_set<string>& ignoredMacros) {
	// Throw an error
	return vector<Token>();
}

FunctionMacro::FunctionMacro(Token _name, unordered_map<string, int> _argumentToIndex) {
	name = _name;
	argumentToIndex = _argumentToIndex;
}

vector<Token> FunctionMacro::expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros) {
	// Throw an error
	return vector<Token>();
}

// Expands a function-like macro. First fully expands the arguments, then substitutes them into the macro body, and finally it expands the whole macro body again.
vector<Token> FunctionMacro::expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& args, unordered_set<string>& ignoredMacros) {
	// Check for correct number of arguments provided
	if (args.size() != argumentToIndex.size()) {
		// Throw error wrong number of args
	}
	
	vector<Token> substitutedExpression;

	// Expand arguments
	for (int i = 0; i < (int)args.size(); i++) {
		args[i] = macroExpandExpression(macros, ignoredMacros, args[i]);
	}

	// Substitute arguments into macro body
	for (int i = 0; i < (int)value.size(); i++) {
		Token& token = value[i];
		if (token.type != TokenType::IDENTIFIER) { substitutedExpression.push_back(token); continue; }
		// Expand variables
		if (argumentToIndex.contains(token.getLexeme())) {
			int argIndex = argumentToIndex[token.getLexeme()];
			substitutedExpression.insert(substitutedExpression.end(), args[argIndex].begin(), args[argIndex].end());
		}
		// Non-macro identifier
		else {
			substitutedExpression.push_back(token);
		}
	}

	ignoredMacros.insert(name.getLexeme());

	vector<Token> expandedExpression = macroExpandExpression(macros, ignoredMacros, substitutedExpression);

	ignoredMacros.erase(name.getLexeme());

	return expandedExpression;
}

Preprocessor::Preprocessor(){
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

	// Check file validity
	if (p.extension().string() != ".csl" || p.stem().string() != "main" || !exists(p)) {
		errorHandler::addSystemError("Couldn't find main.csl");
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
	
	vector<Token> depsToParse = processDirectivesAndMacros(unit);

	for (const Token& dep : depsToParse) {
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
	for (const Token& token : unit->tokens) {
		std::cout << token.getLexeme() << " " << static_cast<int>(token.type) << "\n";
	}
	return unit;
}

bool isAtEnd(vector<Token>& tokens, int pos) {
	return pos >= tokens.size();
}

void Preprocessor::topsort(CSLModule* unit) {
	//TODO: we can get a stack overflow if this goes into deep recursion, try making a iterative stack based DFS implementation
	unit->traversed = true;
	for (CSLModule* dep : unit->deps) {
		if (!dep->traversed) topsort(dep);
	}
	sortedUnits.push_back(unit);
}

vector<Token> Preprocessor::processDirectivesAndMacros(CSLModule* unit) {
	vector<Token>& tokens = unit->tokens;
	vector<Token> resultTokens; // Tokenization after processing imports and macros
	vector<Token> importTokens;
	unordered_map<string, unique_ptr<Macro>> macros;

	for (int i = 0; i < tokens.size(); i++) {
		Token& token = tokens[i];
		// Exclude whitespace and new line tokens from final tokenization (they are only used for macro processing)
		if (token.type == TokenType::WHITESPACE || token.type == TokenType::NEWLINE) { continue; }

		// Add a macro
		if (token.type == TokenType::ADDMACRO) {
			// Move to macro name (skip a whitespace)
			if (getTypeAt(tokens, ++i) != TokenType::WHITESPACE) { error(tokens[i], "Expected whitespace."); continue; }
			if (getTypeAt(tokens, ++i) != TokenType::IDENTIFIER) { error(tokens[i], "Expected macro name."); continue; }

			Token macroName = tokens[i];
			
			// Check if already declared
			if (macros.contains(macroName.getLexeme())) {
				error(macroName, "Macro redefinition not allowed.");
				continue;
			}

			// Function-like macro
			if (getTypeAt(tokens, i+1) == TokenType::LEFT_PAREN) {
				// Move to opening bracket
				i += 1;

				// Parse arguments
				unordered_map<string, int> argumentToIndex;

				vector<vector<Token>> args = getArguments(tokens, i);

				for (int j = 0; j < (int)args.size(); j++) {
					if (args[j].size() != 1) { error(macroName, "Each macro argument should contain exactly 1 token."); break; }

					string argName = args[j][0].getLexeme();

					if (argumentToIndex.contains(argName)) { error(args[j][0], "Cannot have 2 or more arguments of the same name."); break; }
					
					argumentToIndex[argName] = j;
				}

				macros[macroName.getLexeme()] = std::make_unique<FunctionMacro>(macroName, argumentToIndex);
			}
			// Object-like macro
			else {
				macros[macroName.getLexeme()] = std::make_unique<ObjectMacro>(macroName);
			}

			if (getTypeAt(tokens, i + 1) != TokenType::WHITESPACE) { error(tokens[i + 1], "Expected whitespace."); continue; }

			// Move to macro value
			i += 2;

			// Add tokens to macro value (until newline)
			while (getTypeAt(tokens, i) != TokenType::NEWLINE && getTypeAt(tokens, i) != TokenType::TOKEN_EOF) {
				if (tokens[i].type == TokenType::WHITESPACE) { i++;  continue; }

				macros[macroName.getLexeme()]->value.push_back(tokens[i++]);
			}
		}
		// Remove a macro
		else if (token.type == TokenType::REMOVEMACRO) {
			// Move to macro name (skip a whitespace)
			if (getTypeAt(tokens, ++i) != TokenType::WHITESPACE) { error(tokens[i], "Expected whitespace."); continue; }
			if (getTypeAt(tokens, ++i) != TokenType::IDENTIFIER) { error(tokens[i], "Expected macro name."); continue; }

			Token macroName = tokens[i];
			if (!macros.contains(macroName.getLexeme())) { error(macroName, "Macro wasn't defined yet."); continue; }

			// Remove the macro
			macros.erase(macroName.getLexeme());
		}
		// Add a dependency
		else if (token.type == TokenType::IMPORT) {
			// Move to dependency name
			i += 2;

			if (getTypeAt(tokens, i) != TokenType::STRING) error(tokens[i], "Expected a module name.");

			// Add dependency to list of dependencies
			Token dependencyName = tokens[i];

			importTokens.push_back(dependencyName);
		}
		else if (token.type == TokenType::IDENTIFIER) {
			// Process macro
			if (macros.contains(token.getLexeme())) {
				unordered_set<string> ignoredMacros;
				processMacro(macros, ignoredMacros, resultTokens, tokens, i);
			}
			// Non-macro identifier
			else {
				resultTokens.push_back(token);
			}
		}
		else {
			resultTokens.push_back(token);
		}
	}

	unit->tokens = resultTokens;

	return importTokens;
}

void Preprocessor::error(Token token, string msg) {
	errorHandler::hadError = true;
	errorHandler::addCompileError(msg, token);
}