#pragma once
#include "Common.h"
#include "PageCache.h"
//管理页的大块内存

//span需要设置为双向链表

// 设置单例模式
class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &_sInst;
	}
	//从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
	//获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t size);
	//将一定数量的对象释放到span
	void ReleaseListToSpans(void* start, size_t size);
private:
	SpanList _spanLists[NFREE_LIST];
private:
	CentralCache()
	{}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
	static CentralCache _sInst;
};