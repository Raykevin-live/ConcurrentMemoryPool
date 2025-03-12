#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"

//单例模式静态对象实例化
CentralCache CentralCache::_sInst;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size) {
	size_t index = SizeClass::Index(size);
	//加桶锁
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);
	//从spanList中获取batchNum个对象
	//如果不够batchNum个，有多少拿多少
	start = span->_freeList;
	end = start;
	size_t i = 0;
	size_t actualNum = 1;
	while (i < batchNum - 1 && NextObj(end) != nullptr) {
		end = NextObj(end);
		++i;
		++actualNum;
	}
	
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	//解锁
	_spanLists[index]._mtx.unlock();

	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
	//查看当前的spanlist中是否还有未分配对象的span
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList != nullptr) {
			// 有，则直接返回
			return it;
		}
		else {
			it = it->_next;
		}
	}
	// 有需要将回收的内存并起来的问题，所以需要进入page cache时需要解锁central cache, 这样
	// 其他线程释放内存对象回来时，不会阻塞
	list._mtx.unlock();
	
	// central cache对应位置为空说明没有空闲，只能找page cache
	// 直接在外面加锁
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	// 对获取的span进行切分，不需要加锁，因为这时其他线程访问不到这个span
	//计算span的大块内存的起始地址和大块内存的大小（字节数）
	// 起始地址就是第几个8K
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes; // 找结束地址

	// 把大块内存切完，并挂起来 (尾插的好处，切完用的时候地址是连续的）
	// 1、先切一块去做头，方便尾插
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	//链起来
	while (start < end) {
		NextObj(tail) = start;
		tail = start;
		start += size;
	}
	NextObj(tail) = nullptr;
	//切好后，将span挂到桶里的时候需要加锁
	list._mtx.lock();
	list.PushFront(span);

	return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size) {
	size_t index = SizeClass::Index(size);

	_spanLists[index]._mtx.lock();
	while (start) {
		void* next = NextObj(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		--span->_useCount;

		if (span->_useCount == 0) {
			//说明切分出去的所有小块都回来了
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			//解桶锁，其他thread cache可以进行操作
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();
}
