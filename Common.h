#pragma once
#include <iostream>
#include <cassert>
#include <thread>
#include <mutex>
#include <unordered_map>
//#include <algorithm>

using std::endl;
using std::cout;
//using std::vector;

static const size_t MAX_BYTES = 256 * 1024; //256KB
static const size_t NFREE_LIST = 208; //256KB
static const size_t NPAGES = 129; // ���128�Ѿ�������4������Span����1ӳ�䵽128��0��ӳ�䣩
static const size_t PAGE_SHIFT = 13;// һҳ8k

//��������
#ifdef _WIN32
	#include <Windows.h>
#else
	//Linux
	#include <unistd.h>
#endif

// 32λ��_WIN32 ��64λ�¶���
#ifdef _WIN64
	typedef unsigned long long PAGE_ID;
#elif _WIN32
	typedef size_t PAGE_ID;
#endif

inline static void* SystemAlloc(size_t kpage) {
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage * (1 << PAGE_SHIFT),
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	//Linux : brk mmap
	//void* ptr = 
#endif
	if (ptr == nullptr) {
		throw std::bad_alloc();
	}
	return ptr;
}

inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap�� 
#endif
}

static void*& NextObj(void* obj) {
	return *(void**)obj;
}
//�����кõ�С�ڴ�
class FreeList {
public:
	void Push(void* obj) {
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_size;
	}

	void PushRange(void* start, void* end, size_t size) {
		NextObj(end) = _freeList;
		_freeList = start;
		_size += size;
	}
	void PopRange(void*& start, void*& end, size_t n) {
		assert(n >= _size);
		start = _freeList;
		end = start;

		for (size_t i = 0; i < n - 1; i++) {
			end = NextObj(end);
		}
		_freeList = NextObj(end);
		NextObj(end) = nullptr;
		_size -= n;
	}
	void* Pop() {
		// ͷɾ
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(obj);
		--_size;
		return obj;
	}

	bool Empty() {
		return _freeList == nullptr;
	}
	size_t& MaxSize() {
		return _maxSize;
	}
	size_t Size() {
		return _size;
	}
private:
	void* _freeList = nullptr;
	size_t _maxSize = 1;
	size_t _size = 0;
};

//��������С�Ķ���ӳ�����
class SizeClass {
public:
	/*size_t _RoundUp(size_t size, size_t alignNum) {
		size_t alignSize = 0;
		if (size % 8 != 0) {
			alignSize = (size / alignNum + 1) * alignNum;
		}
		else {
			alignSize = size;
		}
		return alignSize;
	}*/

	//��Ӧ����Ӧ������
	static size_t _RoundUp(size_t size, size_t alignNum) {
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}
	// ������������10%�����ҵĿռ��˷�
	static size_t RoundUp(size_t size) {
		if (size <= 128) {
			return _RoundUp(size, 8);
		}
		else if (size <= 1024) {
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024) {
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024) {
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024) {
			return _RoundUp(size, 8*1024);
		}
		else {
			assert(false);
		}
		return -1;
	}

	//ӳ��λ��
	/*size_t _Index(size_t size, size_t alignNum) {
		if (size % alignNum == 0) {
			return (size % alignNum - 1);
		}
		else {
			return size % alignNum;
		}
	}*/
	static size_t _Index(size_t size, size_t alignShift) {
		return (size + (1 << alignShift) - 1) >> alignShift - 1;
	}
	//ӳ�����
	static size_t Index(size_t size) {
		assert(size <= MAX_BYTES);
		//ǰ��λ�����Ӧ��������
		static int group_array[4] = { 16, 56, 56, 56 };
		if (size <= 128) {
			return _Index(size, 3);
		}
		else if (size <= 1024) {
			return _RoundUp(size-128, 4) + group_array[0];
		}
		else if (size <= 8 * 1024) {
			return _RoundUp(size - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (size <= 64 * 1024) {
			return _RoundUp(size - 8*1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (size <= 256 * 1024) {
			return _RoundUp(size - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else {
			assert(false);
		}
		return -1;
	}

	//һ��thread cache���м仺���ȡ���ٸ��ڴ�С��
	static size_t NumMoveSize(size_t size) {
		// ����ʼ���������㷨
		// С�������㣬������ٵ�(��ֹ��δ���Ͱ����
		assert(size > 0);
		int num = MAX_BYTES / size;
		if (num < 2) {
			num = 2;
		}
		if (num > 512) {
			num = 512;
		}
		return num;
	}

	//����һ����ϵͳ��ȡ��ҳ
	// �������� 8byte
	// ...
	// �������� 256KB
	//size : ��������Ĵ�С
	static size_t NumMovePage(size_t size) {
		size_t num = NumMoveSize(size);
		size_t npage = num * size;

		npage >>= PAGE_SHIFT;
		if (npage == 0) {
			npage = 1;
		}
		return npage;
	}
};


//����������ҳ��ҳ������ڴ��Ƚṹ
//span��Ҫ����Ϊ˫������
struct Span {
	PAGE_ID _pageId = 0; // ����ڴ���ʼҳҳ��
	size_t _n = 0;	//ҳ������

	Span* _next = nullptr;//˫������
	Span* _prev = nullptr;

	size_t _useCount = 0;//�кõ�С���ڴ棬 �������thread cache�ļ���
	void* _freeList = nullptr; // �кõ�С���ڴ�������б�

	bool isUse = false; // �Ƿ��ڱ�ʹ��
};

//��ͷ˫��ѭ������
class SpanList {
public:
	SpanList() {
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}

	Span* Begin() {
		return _head->_next;
	}
	Span* End() {
		return _head;
	}
	void PushFront(Span* span) {
		Insert(Begin(), span);
	}

	//�õ���㣬�����
	Span* PopFront() {
		Span* front = _head->_next;
		Erase(front);
		return front;
	}

	void Insert(Span* pos, Span* newSpan) {
		assert(pos);
		assert(newSpan);

		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_prev = prev;
		newSpan->_next = pos;
		pos->_prev = newSpan;
	}
	void Erase(Span* pos) {
		assert(pos);
		assert(pos != _head);

		Span* prev = pos->_prev;
		Span* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
		//delete pos; //���ﲻҪdelete����Ҫ���ڴ滹��central cache
	}
	bool Empty() {
		return _head->_next == _head;
	}
private:
	Span* _head;
public:
	std::mutex _mtx; // Ͱ��
};