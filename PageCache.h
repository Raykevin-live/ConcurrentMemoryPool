#pragma once
#include "Common.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInstan;
	}
	//��ȡһ��kҳ��Span
	Span* NewSpan(size_t k);
public:
	std::mutex _pageMtx;
private:
	SpanList _SpanLists[NPAGES];
	

	//����ģʽ
	PageCache()
	{};
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _sInstan;
};
