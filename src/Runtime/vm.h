#pragma once
#include "../Codegen/codegenDefs.h"
#include "../Objects/objects.h"
#include "thread.h"
#include <condition_variable>

namespace runtime {
	string expectedType(string msg, Value val);
	
	class VM {
	public:
		VM(compileCore::Compiler* compiler);
		void execute();
		void mark(memory::GarbageCollector* gc);
		bool allThreadsPaused();
		//used by threads
		vector<Globalvar> globals;
		vector<File*> sourceFiles;
		//for adding/removing threads
		std::mutex mtx;
		vector<Thread*> childThreads;

		// For pausing threads during gc run
		std::mutex pauseMtx;
		std::condition_variable mainThreadCv;
		std::condition_variable childThreadsCv;
		std::atomic<byte> threadsPaused;
		Thread* mainThread;
	};

}
