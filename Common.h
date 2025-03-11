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
static const size_t NPAGES = 129; // 最大128已经可以切4个最大的Span（从1映射到128，0不映射）
static const size_t PAGE_SHIFT = 13;// 一页8k

//条件编译
#ifdef _WIN32
	#include <Windows.h>
#else
	//Linux
	#include <unistd.h>
#endif

// 32位下_WIN32 ，64位下都有
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
	// sbrk unmmap等 
#endif
}

static void*& NextObj(void* obj) {
	return *(void**)obj;
}
//管理切好的小内存
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
		// 头删
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

//计算对象大小的对齐映射规则
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

	//对应到对应对齐数
	static size_t _RoundUp(size_t size, size_t alignNum) {
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}
	// 整体控制在最多10%的左右的空间浪费
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

	//映射位置
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
	//映射规则
	static size_t Index(size_t size) {
		assert(size <= MAX_BYTES);
		//前几位区间对应的链个数
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

	//一次thread cache从中间缓存获取多少个内存小块
	static size_t NumMoveSize(size_t size) {
		// 慢开始反馈调节算法
		// 小对象给多点，对象给少点(防止多次触发桶锁）
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

	//计算一次向系统获取几页
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	//size : 单个对象的大小
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


//管理多个连续页的页表，大块内存跨度结构
//span需要设置为双向链表
struct Span {
	PAGE_ID _pageId = 0; // 大块内存起始页页号
	size_t _n = 0;	//页的数量

	Span* _next = nullptr;//双向链表
	Span* _prev = nullptr;

	size_t _useCount = 0;//切好的小块内存， 被分配给thread cache的计数
	void* _freeList = nullptr; // 切好的小块内存的自由列表

	bool isUse = false; // 是否在被使用
};

//带头双向循环链表
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

	//拿到结点，并解掉
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
		//delete pos; //这里不要delete掉，要把内存还给central cache
	}
	bool Empty() {
		return _head->_next == _head;
	}
private:
	Span* _head;
public:
	std::mutex _mtx; // 桶锁
};