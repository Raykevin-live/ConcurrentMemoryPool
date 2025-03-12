#define _CRT_SECURE_NO_WARNINGS 1
#include "CentralCache.h"

//����ģʽ��̬����ʵ����
CentralCache CentralCache::_sInst;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size) {
	size_t index = SizeClass::Index(size);
	//��Ͱ��
	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);
	//��spanList�л�ȡbatchNum������
	//�������batchNum�����ж����ö���
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
	//����
	_spanLists[index]._mtx.unlock();

	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
	//�鿴��ǰ��spanlist���Ƿ���δ��������span
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList != nullptr) {
			// �У���ֱ�ӷ���
			return it;
		}
		else {
			it = it->_next;
		}
	}
	// ����Ҫ�����յ��ڴ沢���������⣬������Ҫ����page cacheʱ��Ҫ����central cache, ����
	// �����߳��ͷ��ڴ�������ʱ����������
	list._mtx.unlock();
	
	// central cache��Ӧλ��Ϊ��˵��û�п��У�ֻ����page cache
	// ֱ�����������
	PageCache::GetInstance()->_pageMtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	// �Ի�ȡ��span�����з֣�����Ҫ��������Ϊ��ʱ�����̷߳��ʲ������span
	//����span�Ĵ���ڴ����ʼ��ַ�ʹ���ڴ�Ĵ�С���ֽ�����
	// ��ʼ��ַ���ǵڼ���8K
	char* start = (char*)(span->_pageId << PAGE_SHIFT);
	size_t bytes = span->_n << PAGE_SHIFT;
	char* end = start + bytes; // �ҽ�����ַ

	// �Ѵ���ڴ����꣬�������� (β��ĺô��������õ�ʱ���ַ�������ģ�
	// 1������һ��ȥ��ͷ������β��
	span->_freeList = start;
	start += size;
	void* tail = span->_freeList;
	//������
	while (start < end) {
		NextObj(tail) = start;
		tail = start;
		start += size;
	}
	NextObj(tail) = nullptr;
	//�кú󣬽�span�ҵ�Ͱ���ʱ����Ҫ����
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
			//˵���зֳ�ȥ������С�鶼������
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			//��Ͱ��������thread cache���Խ��в���
			_spanLists[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

		}
		start = next;
	}
	_spanLists[index]._mtx.unlock();
}
