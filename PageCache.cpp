#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

PageCache PageCache::_sInstan;

//对应桶里没有，要往大的桶找，切完部分返回，部分挂起来
//刚开始Page上不挂桶，每次都只向堆申请128page span
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	//大于128页直接向堆申请
	if (k > NPAGES - 1) {
		void* ptr = SystemAlloc(k);
		//Span* span = new Span;
		Span* span = _spanPool.New();
		span->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_n = k;
		//缓存页号
		_idSpanMap[span->_pageId] = span;

		return span;
	}
	//先检查第k个桶里面有没有span
	if (!_SpanLists[k].Empty()) {
		return _SpanLists[k].PopFront();
	}
	//检查后面的桶里有没有span，如果有可以进行切分
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_SpanLists[i].Empty()) {
			//切分
			Span* nSpan = _SpanLists[i].PopFront();
			/*Span* kSpan = new Span;*/
			Span* kSpan = _spanPool.New();

			//在nSpan的头部切k页
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;
			//剩余的挂上去
			_SpanLists[nSpan->_n].PushFront(nSpan);
			//存储nSpan的首位页号和nSpan的映射, 方便page cache回收内存时进行合并查找
			_idSpanMap[nSpan->_pageId] = nSpan;
			_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;


			// 建立id和span的映射，方便central cache回收小块内存时查找对应的span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i) {
				_idSpanMap[kSpan->_pageId + i] = kSpan;
			}
			return kSpan;
		}
	}
	//后面没有大页的span
	//要从堆去申请
	Span* bigSpan = _spanPool.New();
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_SpanLists[bigSpan->_n].PushFront(bigSpan);
	//开完空间再去申请
	//但是要注意递归时被锁住的问题
	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
	//访问哈希表也要加锁，防止数据被删除了还在读
	std::unique_lock<std::mutex> lock(_pageMtx); //出作用域自动解锁 RAII机制
	auto ret = _idSpanMap.find(id);
	if (ret != _idSpanMap.end()) {
		return ret->second;
	}
	else {
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* span) {
	// 解决外碎片问题
	// 大于128页直接还给堆
	if (span->_n > NPAGES - 1) {
		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);
		return;
	}

	// 先要对span先后的页尝试进行合并，缓解外碎片问题
	// 
	// 向前合并	
	while (1) {
		PAGE_ID prevId = span->_pageId - 1;
		// 前面的页号没有了，不合并了
		auto ret = _idSpanMap.find(prevId);
		if (ret == _idSpanMap.end()) {
			break;
		}
		//前面相邻页在使用，不合并
		Span* prevSpan = ret->second;
		if (prevSpan->isUse == true) {
			break;
		}
		//合并超过128页的span没法管理，不合并
		if (prevSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_SpanLists[prevSpan->_n].Erase(prevSpan);
		//delete prevSpan;
		_spanPool.Delete(prevSpan);
	}

	//向后合并
	while (1) {
		PAGE_ID nextId = span->_pageId - span->_n;
		// 前面的页号没有了，不合并了
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end()) {
			break;
		}
		//后面相邻页在使用，不合并
		Span* nextSpan = ret->second;
		if (nextSpan->isUse == true) {
			break;
		}
		//合并超过128页的span没法管理，不合并
		if (nextSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		span->_n += nextSpan->_n;

		_SpanLists[nextSpan->_n].Erase(nextSpan);
		//delete nextSpan;
		_spanPool.Delete(nextSpan);
	}

	//还回PageCache
	_SpanLists[span->_n].PushFront(span);
	span->isUse = false;
	//存首位方便合并
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId+span->_n-1] = span;
}
