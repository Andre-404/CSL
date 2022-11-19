#include "preprocessor.h"
#include <filesystem>
#include "../files.h"
#include <iostream>
#include "../ErrorHandling/errorHandler.h"


using std::unordered_set;
using std::unordered_map;
using namespace preprocessing;
using namespace errorHandler;


// Helper functions for preprocessor and macros

// Securely retrieve type of tokens[pos]
TokenType getTypeAt(const vector<Token>& tokens, const int pos) {
	if (pos < 0 || pos >= tokens.size()) return TokenType::TOKEN_EOF;
	return tokens[pos].type;
}

// Takes iterator to function-like macros name, retrieves arguments (if they exist)
vector<vector<Token>> getArguments(vector<Token> source, int& i) {
	Token macroName = source[i];

	int bracketBalance = 0;
	vector<vector<Token>> args = { {} };

	// Move to '('
	i++;
	if (getTypeAt(source, i) == TokenType::WHITESPACE) i++;

	
	if (getTypeAt(source, i) != TokenType::LEFT_PAREN) {
		// Expected arguments after function-like macro
	}

	// Retrieve arguments
	do {
		switch (getTypeAt(source, i)) {
			case TokenType::LEFT_PAREN:
				if (bracketBalance > 0) args.back().push_back(source[i]);
				bracketBalance++;
				break;
			case TokenType::RIGHT_PAREN:
				bracketBalance--;
				if (bracketBalance > 0) args.back().push_back(source[i]);
				break;
			case TokenType::COMMA:
				// Check if comma is splitting macro arguments
				if (bracketBalance == 1) { args.push_back(vector<Token>()); }
				else args.back().push_back(source[i]);
				break;
			case TokenType::WHITESPACE:
				break;
			default:
				args.back().push_back(source[i]);
				break;
		}
		i++;
	} while (getTypeAt(source, i) != TokenType::TOKEN_EOF && bracketBalance > 0);
	
	// Make sure i is at the ending bracket
	i--;

	if (bracketBalance != 0) {
		addCompileError("Unterminated argument sequence invoking macro.", macroName);
	}

	return args;
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
			vector<Token> expandedMacro = macros[lexeme]->expand(macros, ignoredMacros, expression, i);
			result.insert(result.end(), expandedMacro.begin(), expandedMacro.end());
		}
		// Non-macro identifier
		else {
			result.push_back(token);
		}
	}
	return result;
}

ObjectMacro::ObjectMacro(Token _name) { name = _name; }

vector<Token> ObjectMacro::expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros, vector<Token>& source, int& i) {
	ignoredMacros.insert(name.getLexeme());

	vector<Token> expanded = macroExpandExpression(macros, ignoredMacros, value);

	ignoredMacros.erase(name.getLexeme());

	return expanded;
}

FunctionMacro::FunctionMacro(Token _name, unordered_map<string, int> _argumentToIndex) {
	name = _name;
	argumentToIndex = _argumentToIndex;
}

// Expands a function-like macro. First fully expands the arguments, then substitutes them into the macro body, and finally it expands the whole macro body again.
vector<Token> FunctionMacro::expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros, vector<Token>& source, int& i) {
	Token macroName = source[i];
	
	// Retrieve arguments for macro
	vector<vector<Token>> args = getArguments(source, i);
	
	// Check for correct number of arguments provided
	if (args.size() != argumentToIndex.size()) {
		addCompileError(std::format("Macro requires {} arguments, but was provided with {}.", argumentToIndex.size(), args.size()), macroName);
		return vector<Token>();
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
	curUnit = nullptr;
}

Preprocessor::~Preprocessor() {
	//for (preprocessUnit* pUnit : sortedUnits) delete pUnit;
}

void Preprocessor::preprocessProject(string mainFilePath) {
	using namespace std::filesystem;
	path p(mainFilePath);

	// Check file validity
	if (p.extension().string() != ".csl" || p.stem().string() != "main" || !exists(p)) {
		errorHandler::addSystemError("Couldn't find main.csl");
		return;
	}

	projectRootPath = p.parent_path().string() + "/";
	CSLModule* mainModule = scanFile("main.csl");
	topsort(mainModule);
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
				addCompileError("Cyclical importing detected.", dep);
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
			addCompileError("File " + depName + " doesn't exist.", dep);
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
			if (getTypeAt(tokens, ++i) != TokenType::WHITESPACE) { addCompileError("Expected whitespace.", tokens[i]); continue; }
			if (getTypeAt(tokens, ++i) != TokenType::IDENTIFIER) { addCompileError("Expected macro name.", tokens[i]); continue; }

			Token macroName = tokens[i];
			
			// Check if the macro was already declared
			if (macros.contains(macroName.getLexeme())) { addCompileError("Macro redefinition not allowed.", macroName); continue; }

			// Function-like macro
			if (getTypeAt(tokens, i+1) == TokenType::LEFT_PAREN) {
				// Parse arguments
				unordered_map<string, int> argumentToIndex;

				vector<vector<Token>> args = getArguments(tokens, i);

				for (int j = 0; j < (int)args.size(); j++) {
					if (args[j].size() != 1 || args[j][0].type != TokenType::IDENTIFIER) { addCompileError("Each macro argument should be a single identifier token.", macroName); break; }

					string argName = args[j][0].getLexeme();

					if (argumentToIndex.contains(argName)) { addCompileError("Cannot have 2 or more arguments of the same name.", args[j][0]); break; }
					
					argumentToIndex[argName] = j;
				}

				macros[macroName.getLexeme()] = std::make_unique<FunctionMacro>(macroName, argumentToIndex);
			}
			// Object-like macro
			else {
				macros[macroName.getLexeme()] = std::make_unique<ObjectMacro>(macroName);
			}

			if (getTypeAt(tokens, i + 1) != TokenType::WHITESPACE) { addCompileError("Expected whitespace.", tokens[i + 1]); continue; }

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
			if (getTypeAt(tokens, ++i) != TokenType::WHITESPACE) { addCompileError("Expected whitespace.", tokens[i]); continue; }
			if (getTypeAt(tokens, ++i) != TokenType::IDENTIFIER) { addCompileError("Expected macro name.", tokens[i]); continue; }

			Token macroName = tokens[i];
			if (!macros.contains(macroName.getLexeme())) { addCompileError("Cannot remove a macro that wasn't declared yet.", macroName); continue; }

			// Remove the macro
			macros.erase(macroName.getLexeme());
		}
		// Add a dependency
		else if (token.type == TokenType::IMPORT) {
			// Move to dependency name
			i += 2;

			if (getTypeAt(tokens, i) != TokenType::STRING) addCompileError("Expected a module name.", tokens[i]);

			// Add dependency to list of dependencies
			Token dependencyName = tokens[i];

			importTokens.push_back(dependencyName);
		}
		else if (token.type == TokenType::IDENTIFIER) {
			string lexeme = token.getLexeme();
			// Process macro
			if (macros.contains(lexeme)) {
				unordered_set<string> ignoredMacros;
				vector<Token> expanded = macros[lexeme]->expand(macros, ignoredMacros, tokens, i);
				resultTokens.insert(resultTokens.end(), expanded.begin(), expanded.end());
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