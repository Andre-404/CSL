#pragma once
#include "../MemoryManagment/heapObject.h"
#include "../MemoryManagment/garbageCollector.h"
#include <immintrin.h>
#include "../ErrorHandling/errorHandler.h"
#include <iostream>
#include <format>

class ArrHeader : public memory::HeapObject {
	public:
		uInt64 size;
		ArrHeader(uInt64 _size, uInt64 _sizeOfType) {
			size = _size;
			sizeOfType = _sizeOfType;
		}

		static ArrHeader* createArr(uInt64 sizeOfType, uInt64 length) {
			void* ptr = memory::__allocObj(sizeof(ArrHeader) + length * sizeOfType);
			return new(ptr) ArrHeader(length, sizeOfType);
		}

		void move(byte* newAddress) {
			memmove(newAddress, this, getSize());
		}
		void updateInternalPointers() {
			//nothing to update
		}
		uInt64 getSize() {
			//+1 for terminator byte
			return sizeof(ArrHeader) + size * sizeOfType;
		}
		void mark() {
			//none
		}
		byte* getPtr() {
			return reinterpret_cast<byte*>(this) + sizeof(ArrHeader);
		}
	private:
		uInt64 sizeOfType;
	};


//header for array of type T
template<typename T>
class ManagedArray {
public:
	ManagedArray() {
		count = 0;
		header = nullptr;
	}

	ManagedArray(uInt64 size) {
		count = size;
		header = nullptr;
		resize(size);
		T* arr = reinterpret_cast<T*>(header->getPtr());
		for (int i = 0; i < size; i++) arr[i] = T();
	}

	ManagedArray(uInt64 size, T val) {
		count = size;
		header = nullptr;
		resize(size);
		T* arr = reinterpret_cast<T*>(header->getPtr());
		for (int i = 0; i < size; i++) arr[i] = val;
	}

	void push(T item) {
		checkSize(count + 1);
		T* arr = reinterpret_cast<T*>(header->getPtr()) + count;
		*arr = item;
		count++;
	}

	void remove(uInt64 index) {
		//removes the item and slides all items above index down by 1 position
		if (index >= count) {
			std::cout << std::format("Tried deleting item at index {}, array size is {}.", index, count);
			exit(64);
		}
		T* arr = reinterpret_cast<T*>(header->getPtr()) + index + 1;
		if(count > index + 1) memmove(reinterpret_cast<T*>(header->getPtr()) + index, arr, (count - (index + 1))*sizeof(T));
		count--;
	}

	//appends contents from 'other' to the end of 'this'
	void addAll(ManagedArray<T>& other) {
		checkSize(count + other.count);
		
		T* from = reinterpret_cast<T*>(other.header->getPtr());
		T* to = reinterpret_cast<T*>(header->getPtr()) + count;
		memcpy(to, from, other.count * sizeof(T));
		count = count + other.count;
	}

	void insert(T item, uInt64 index) {
		if (index >= count) {
			std::cout << std::format("Tried inserting item at index {}, array size is {}.", index, count);
			exit(64);
		}
		checkSize(count + 1);
		//moves everything above index one place above, then inserts item at index
		T* from = reinterpret_cast<T*>(header->getPtr()) + index;
		T* to = reinterpret_cast<T*>(header->getPtr()) + index + 1;
		memmove(to, from, (count - index) * sizeof(T));
		*from = item;
		count++;
	}

	T operator[](uInt64 index) const {
		if (index >= count) {
			std::cout << std::format("Tried accessing item at index {}, array size is {}.", index, count);
			exit(64);
		}

		return *(reinterpret_cast<T*>(header->getPtr()) + index);
	}

	T& operator[](uInt64 index) {
		if (index >= count) {
			std::cout << std::format("Tried accessing item at index {}, array size is {}.", index, count);
			exit(64);
		}

		return *(reinterpret_cast<T*>(header->getPtr()) + index);
	}

	ManagedArray<T>& operator=(ManagedArray<T> other) {
		if (this == &other) return *this;

		header = std::exchange(other.header, nullptr);
		count = std::exchange(other.count, 0);
		return *this;
	}

	uInt64 size() {
		return count;
	}

	void mark() {
		memory::gc.markObj(header);
	}

	void updateInternalPtr() {
		header = reinterpret_cast<ArrHeader*>(header->moveTo);
	}
private:
	ArrHeader* header;
	//this isn't the maximum array size, just how much is in use
	uInt64 count;

	//allocates new array and copies the contents of the previous one
	void resize(uInt64 newSize) {
		//if the new size is the same as old, don't do anything
		if (header != nullptr && header->size == newSize) return;
		//new size is always a power of 2
		uInt64 size = (1ll << (64 - _lzcnt_u64(newSize - 1)));
		ArrHeader* newHeader = ArrHeader::createArr(sizeof(T), size);
		if(header != nullptr) memcpy(newHeader->getPtr(), header->getPtr(), count * sizeof(T));
		header = newHeader;
	}

	void checkSize(uInt64 newSize) {
		if (header == nullptr || newSize > header->size) resize(newSize);
	}
};