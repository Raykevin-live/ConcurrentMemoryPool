#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

PageCache PageCache::_sInstan;

//��ӦͰ��û�У�Ҫ�����Ͱ�ң����겿�ַ��أ����ֹ�����
//�տ�ʼPage�ϲ���Ͱ��ÿ�ζ�ֻ�������128page span
Span* PageCache::NewSpan(size_t k) {
	assert(k > 0 && k<NPAGES);
	//�ȼ���k��Ͱ������û��span
	if (!_SpanLists[k].Empty()) {
		return _SpanLists[k].PopFront();
	}
	//�������Ͱ����û��span������п��Խ����з�
	for (size_t i = k + 1; i < NPAGES; i++) {
		if (!_SpanLists[i].Empty()) {
			//�з�
			Span* nSpan = _SpanLists[i].PopFront();
			Span* kSpan = new Span;

			//��nSpan��ͷ����kҳ
			kSpan->_pageId = nSpan->_pageId;
			kSpan->_n = k;

			nSpan->_pageId += k;
			nSpan->_n -= k;
			//ʣ��Ĺ���ȥ
			_SpanLists[nSpan->_n].PushFront(nSpan);
			//�洢nSpan����λҳ�ź�nSpan��ӳ��, ����page cache�����ڴ�ʱ���кϲ�����
			_idSpanMap[nSpan->_pageId] = nSpan;
			_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;


			// ����id��span��ӳ�䣬����central cache����С���ڴ�ʱ���Ҷ�Ӧ��span
			for (PAGE_ID i = 0; i < kSpan->_n; ++i) {
				_idSpanMap[kSpan->_pageId + i] = kSpan;
			}
			return kSpan;
		}
	}
	//����û�д�ҳ��span
	//Ҫ�Ӷ�ȥ����
	Span* bigSpan = new Span;
	void* ptr = SystemAlloc(NPAGES - 1);
	bigSpan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
	bigSpan->_n = NPAGES - 1;
	_SpanLists[bigSpan->_n].PushFront(bigSpan);
	//����ռ���ȥ����
	//����Ҫע��ݹ�ʱ����ס������
	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);
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
	// �������Ƭ����
	// ��Ҫ��span�Ⱥ��ҳ���Խ��кϲ�����������Ƭ����
	// 
	// ��ǰ�ϲ�	
	while (1) {
		PAGE_ID prevId = span->_pageId - 1;
		// ǰ���ҳ��û���ˣ����ϲ���
		auto ret = _idSpanMap.find(prevId);
		if (ret == _idSpanMap.end()) {
			break;
		}
		//ǰ������ҳ��ʹ�ã����ϲ�
		Span* prevSpan = ret->second;
		if (prevSpan->isUse == true) {
			break;
		}
		//�ϲ�����128ҳ��spanû���������ϲ�
		if (prevSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;

		_SpanLists[prevSpan->_n].Erase(prevSpan);
		delete prevSpan;
	}

	//���ϲ�
	while (1) {
		PAGE_ID nextId = span->_pageId - span->_n;
		// ǰ���ҳ��û���ˣ����ϲ���
		auto ret = _idSpanMap.find(nextId);
		if (ret == _idSpanMap.end()) {
			break;
		}
		//��������ҳ��ʹ�ã����ϲ�
		Span* nextSpan = ret->second;
		if (nextSpan->isUse == true) {
			break;
		}
		//�ϲ�����128ҳ��spanû���������ϲ�
		if (nextSpan->_n + span->_n > NPAGES - 1) {
			break;
		}

		span->_n += nextSpan->_n;

		_SpanLists[nextSpan->_n].Erase(nextSpan);
		delete nextSpan;
	}

	//����PageCache
	_SpanLists[span->_n].PushFront(span);
	span->isUse = false;
	//����λ����ϲ�
	_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId+span->_n-1] = span;
}
