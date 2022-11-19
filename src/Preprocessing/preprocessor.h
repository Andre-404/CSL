#pragma once
#include "../common.h"
#include "scanner.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace preprocessing {
	using std::unordered_set;
	using std::unordered_map;
	using std::unique_ptr;
	class Macro {
	public:
		bool isExpanded = false;
		Token name;
		std::vector<Token> value;
		// Expand object-like macro
		virtual vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros) = 0;
		// Expand function-like macro
		virtual vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& arguments, unordered_set<string>& ignoredMacros) = 0;
	};

	class ObjectMacro : public Macro {
	public:
		ObjectMacro() {};
		ObjectMacro(Token _name);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& args, unordered_set<string>& ignoredMacros);
	};

	class FunctionMacro : public Macro {
	public:
		unordered_map<string, int> argumentToIndex;

		FunctionMacro() {};
		FunctionMacro(Token _name, unordered_map<string, int> _argumentToIndex);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, vector<vector<Token>>& args, unordered_set<string>& ignoredMacros);
	};

	class Preprocessor {
	public:
		Preprocessor();
		~Preprocessor();
		void preprocessProject(string mainFilePath);

		vector<CSLModule*> getSortedUnits() { return sortedUnits; }
	private:
		string projectRootPath;
		Scanner scanner;
		CSLModule* curUnit;

		unordered_map<string, CSLModule*> allUnits;
		vector<CSLModule*> sortedUnits;

		vector<Token> processDirectivesAndMacros(CSLModule* unit);

		CSLModule* scanFile(string unitName);
		void topsort(CSLModule* unit);

		void error(Token token, string msg);
	};

}