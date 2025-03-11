#pragma once
#include "Common.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInstan;
	}
	//获取一个k页的Span
	Span* NewSpan(size_t k);
public:
	std::mutex _pageMtx;
private:
	SpanList _SpanLists[NPAGES];
	

	//单例模式
	PageCache()
	{};
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _sInstan;
};
