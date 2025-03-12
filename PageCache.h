#pragma once
#include "Common.h"
#include "PageMap.h"
class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInstan;
	}
	//获取一个k页的Span
	Span* NewSpan(size_t k);
	Span* MapObjectToSpan(void* obj);

	//接过从central cache来的整块span，并放到对应位置
	//释放空空闲span回到pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

public:
	std::mutex _pageMtx;
private:
	SpanList _SpanLists[NPAGES];
	ObjectPool<Span> _spanPool; //用来优化new
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32-PAGE_SHIFT> _idSpanMap;

	//单例模式
	PageCache()
	{};
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _sInstan;
};
