#pragma once
#include "../common.h"
#include "scanner.h"
#include "../ErrorHandling/errorHandler.h"
#include <unordered_map>
#include <tuple>

struct Macro {
	Token name;
	std::vector<Token> value;
	bool isFunctionLike = false;
	//only for function like macros
	std::vector<Token> params;

	Macro() {};
	Macro(Token _name) : name(_name) {};
};

class Preprocessor {
public:
	Preprocessor(ErrorHandler& handler);
	~Preprocessor();
	bool preprocessProject(string mainFilePath);

	vector<CSLModule*> getSortedUnits() { return sortedUnits; }

	bool hadError;
private:
	string projectRootPath;
	Scanner scanner;
	ErrorHandler& errorHandler;
	CSLModule* curUnit;

	std::unordered_map<string, CSLModule*> allUnits;
	vector<CSLModule*> sortedUnits;

	std::tuple<vector<Token>, std::unordered_map<string, Macro>> processDirectives(CSLModule* unit);

	//expands a normal macro
	vector<Token> expandMacro(Macro& toExpand, std::unordered_map<string, Macro>& macros, vector<Macro>& macroStack);
	//expands a function like macro
	vector<Token> expandMacro(Macro& toExpand, vector<Token>& tokens, int pos, std::unordered_map<string, Macro>& macros, vector<Macro>& macroStack);
	void replaceMacros(vector<Token>& tokens, std::unordered_map<string, Macro>& macros, vector<Macro>& macroStack);

	CSLModule* scanFile(string unitName);
	void topsort(CSLModule* unit);

	void error(Token token, string msg);
};