#pragma once

namespace object {
	class Obj;
}

enum class ValueType {
	NUM,
	BOOL,
	OBJ,
	NIL
};

struct Value {
	ValueType type;
	union {
		double number;
		bool boolean;
		object::Obj* object;
	} as;
	Value() {
		type = ValueType::NIL;
		as.object = nullptr;
	}

	Value(double num) {
		type = ValueType::NUM;
		as.number = num;
	}

	Value(bool _bool) {
		type = ValueType::BOOL;
		as.boolean = _bool;
	}

	Value(object::Obj* _object) {
		type = ValueType::OBJ;
		as.object = _object;
	}
	
	static Value nil() {
		return Value();
	}

	bool equals(Value other) {
		return false;
	}
};