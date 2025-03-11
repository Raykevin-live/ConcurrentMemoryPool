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
