#pragma once
#include "Common.h"
#include "PageCache.h"
//����ҳ�Ĵ���ڴ�

//span��Ҫ����Ϊ˫������

// ���õ���ģʽ
class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &_sInst;
	}
	//�����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);
	//��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t size);
	//��һ�������Ķ����ͷŵ�span
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