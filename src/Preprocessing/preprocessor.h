#pragma once
#include "../common.h"
#include "scanner.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <tuple>

namespace preprocessing {
	using std::unordered_set;
	using std::unordered_map;
	using std::unique_ptr;
	using std::tuple;

	class Macro {
	public:
		bool isExpanded = false;
		Token name;
		std::vector<Token> value;
		// Expand macro into destination
		virtual vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros, vector<Token>& source, int& i) = 0;
	};

	class ObjectMacro : public Macro {
	public:
		ObjectMacro() {};
		ObjectMacro(Token _name);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros, vector<Token>& source, int& i);
	};

	class FunctionMacro : public Macro {
	public:
		unordered_map<string, int> argumentToIndex;

		FunctionMacro() {};
		FunctionMacro(Token _name, unordered_map<string, int> _argumentToIndex);

		vector<Token> expand(unordered_map<string, unique_ptr<Macro>>& macros, unordered_set<string>& ignoredMacros, vector<Token>& source, int& i);
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
		
		vector<tuple<Token, Token>> processDirectivesAndMacros(CSLModule* unit);

		CSLModule* scanFile(string unitName);
		void topsort(CSLModule* unit);
	};

}