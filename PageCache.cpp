#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

PageCache PageCache::_sInstan;

//对应桶里没有，要往大的桶找，切完部分返回，部分挂起来
//刚开始Page上不挂桶，每次都只向堆申请128page span
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0 && k<NPAGES);
	//先检查第k个桶里面有没有span
	if (!_SpanLists[k].Empty()) {
		return _SpanLists[k].PopFront();
	}
	//检查后面的桶里有没有span，如果有可以进行切分
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_SpanLists[i].Empty()) {
			//切分
			Span* nSpan = _SpanLists[i].PopFront();
			Span* kSpan = new Span;

			//在nSpan的头部切k页
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;
			//剩余的挂上去
			_SpanLists[nSpan->_n].PushFront(nSpan);
			return kSpan;
		}
	}
	//后面没有大页的span
	//要从堆去申请
	Span* bigSpan = new Span;
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_SpanLists[bigSpan->_n].PushFront(bigSpan);
	//开完空间再去申请
	//但是要注意递归时被锁住的问题
	return NewSpan(k);
}
