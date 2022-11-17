#pragma once
#include "../common.h"
#include "scanner.h"
#include "../ErrorHandling/errorHandler.h"
#include <unordered_map>
#include <map>
#include <string>
#include <tuple>
#include <queue>
#include <memory>

using std::unordered_map;
using std::unique_ptr;

namespace preprocessing {
	class Macro {
	public:
		bool isExpanded = false;
		Token name;
		std::vector<Token> value;
		// Expand object-like macro
		virtual vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, const string& ignoredMacro) = 0;
		// Expand function-like macro
		virtual vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& arguments, const string& ignoredMacro) = 0;
	};

	class ObjectMacro : public Macro {
	public:
		ObjectMacro() {};
		ObjectMacro(Token _name);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, const string& ignoredMacro);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& args, const string& ignoredMacro);
	};

	class FunctionMacro : public Macro {
	public:
		unordered_map<string, int> argumentToIndex;

		FunctionMacro() {};
		FunctionMacro(Token _name, unordered_map<string, int> _argumentToIndex);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, const string& ignoredMacro);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& args, const string& ignoredMacro);
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

		unordered_map<string, CSLModule*> allUnits;
		vector<CSLModule*> sortedUnits;

		vector<Token> processDirectivesAndMacros(CSLModule* unit);

		CSLModule* scanFile(string unitName);
		void topsort(CSLModule* unit);

		void error(Token token, string msg);
	};

}