#include "preprocessor.h"
#include <filesystem>
#include "../files.h"
#include <iostream>
#include "../ErrorHandling/errorHandler.h"


using std::unordered_set;
using std::unordered_map;
using namespace preprocessing;
using namespace errorHandler;

TokenType getTypeAt(const vector<Token>& tokens, const int pos) {
	if (pos < 0 || pos >= tokens.size()) return TokenType::TOKEN_EOF;
	return tokens[pos].type;
}


Preprocessor::Preprocessor(){
	projectRootPath = "";
	curUnit = nullptr;
}

Preprocessor::~Preprocessor() {
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
	
	vector<tuple<Token, Token>> depsToParse = processDirectivesAndMacros(unit);

	for (const tuple<Token, Token>& dep : depsToParse) {
		auto &[path, alias] = dep;
		//since import names are strings, we get rid of ""
		string depName = path.getLexeme();
		depName.erase(0, 1);
		depName.erase(depName.size() - 1, depName.size());
		//if we have already scanned module with name 'dep' we add it to the deps list of this module
		//to topsort it later
		if (allUnits.count(depName) > 0) {
			//if we detect a cyclical import we still continue parsing other files to detect as many errors as possible
			if (!allUnits[depName]->resolvedDeps) {
				addCompileError("Cyclical importing detected.", path);
				continue;
			}
			unit->deps.push_back(Dependency(alias, path, allUnits[depName]));
			continue;
		}
		std::filesystem::path p(projectRootPath + depName);
		if (std::filesystem::exists(p)) {
			Dependency dep = Dependency(alias, path, scanFile(depName));
			unit->deps.push_back(dep);
		}
		else {
			addCompileError("File " + depName + " doesn't exist.", path);
		}
	}
	unit->resolvedDeps = true;
	return unit;
}

bool isAtEnd(vector<Token>& tokens, int pos) {
	return pos >= tokens.size();
}

void Preprocessor::topsort(CSLModule* unit) {
	//TODO: we can get a stack overflow if this goes into deep recursion, try making a iterative stack based DFS implementation
	unit->traversed = true;
	for (Dependency& dep : unit->deps) {
		if (!dep.module->traversed) topsort(dep.module);
	}
	sortedUnits.push_back(unit);
}

vector<tuple<Token, Token>> Preprocessor::processDirectivesAndMacros(CSLModule* unit) {
	vector<Token>& tokens = unit->tokens;
	vector<Token> resultTokens; // Tokenization after processing imports and macros
	vector<tuple<Token, Token>> importTokens;

	auto getErrorToken = [&](int i) {
		if (i < 0 || tokens.size() <= i) return tokens[tokens.size() - 1];
		return tokens[i];
	};

	for (int i = 0; i < tokens.size(); i++) {
		Token& token = tokens[i];
		// Exclude whitespace and new line tokens from final tokenization (they are only used for macro processing)
		if (token.type == TokenType::WHITESPACE || token.type == TokenType::NEWLINE) { continue; }
		// Add a dependency
		if (token.type == TokenType::IMPORT) {
			// Move to dependency name
			if (getTypeAt(tokens, i + 1) == TokenType::WHITESPACE) i += 2;
			else {
				addCompileError("Expected whitespace.", getErrorToken(i + 1));
				continue;
			}

			if (getTypeAt(tokens, i) != TokenType::STRING) {
				addCompileError("Expected a module name.", getErrorToken(i));
				continue;
			}

			// Add dependency to list of dependencies
			Token dependencyName = tokens[i];
			Token alias;
			if (getTypeAt(tokens, i + 2) == TokenType::AS) {
				if (getTypeAt(tokens, i + 4) == TokenType::IDENTIFIER) alias = tokens[i + 4];
				else {
					addCompileError("Expected alias for module. ", getErrorToken(i + 4));
					continue;
				}
				i += 4;
			}

			importTokens.push_back(std::make_tuple(dependencyName, alias));
		}
		else resultTokens.push_back(token);
	}

	unit->tokens = resultTokens;

	return importTokens;
}