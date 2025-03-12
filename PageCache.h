#pragma once
#include "Common.h"
#include "PageMap.h"
class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInstan;
	}
	//��ȡһ��kҳ��Span
	Span* NewSpan(size_t k);
	Span* MapObjectToSpan(void* obj);

	//�ӹ���central cache��������span�����ŵ���Ӧλ��
	//�ͷſտ���span�ص�pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);

public:
	std::mutex _pageMtx;
private:
	SpanList _SpanLists[NPAGES];
	ObjectPool<Span> _spanPool; //�����Ż�new
	//std::unordered_map<PAGE_ID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32-PAGE_SHIFT> _idSpanMap;

	//����ģʽ
	PageCache()
	{};
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _sInstan;
};
