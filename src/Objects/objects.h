#pragma once
#include "../MemoryManagment/heapObject.h"
namespace object {

	enum class ObjType {
		STRING
	};

	class Obj : public memory::HeapObject{
	public:
		ObjType type;
	};


	//this is a header which is followed by the bytes of the string
	class CSLString : public Obj {
	public:
		uInt64 size;
		uInt64 hash;

		CSLString(uInt64 length);

		static CSLString* createString(char* from, uInt64 length);

		void move(byte* newAddress);
		void updateInteralPointers();
		uInt64 getSize();
		void mark(vector<HeapObject*>& stack);

		char* getString();

		bool compare(CSLString* other);

		bool compare(string other);

		CSLString* concat(CSLString* other);
	};

}