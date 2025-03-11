#pragma once
#include "Common.h"


void* SystemAlloc(size_t kpage);

template<class T>
class ObjectPool {
public:
	T* New() {
		T* obj = nullptr;
		if (_freeList!=nullptr) {
			obj = (T*)_freeList;
			_freeList = *((void**)_freeList);
		}
		else {
			if (_remaindBytes < sizeof(T)) {
				_remaindBytes = 1024 * 128;//64KB
				_memory = (char*)SystemAlloc(_remaindBytes >> 13);
				if (_memory == nullptr) {
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remaindBytes -= objSize;
		}
		//if (obj == nullptr) {
		//	throw std::bad_alloc(); // ��� obj ��Ч���׳��쳣
		//}
		new (obj)T;
		return obj;
	}
	void Delete(T* obj) {
		obj->~T();
		*(void**)obj = _freeList;
		_freeList = obj;
	}
private:
	char* _memory = nullptr;
	size_t _remaindBytes = 0;
	void* _freeList = nullptr;
};
