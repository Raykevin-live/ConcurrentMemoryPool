#pragma once
#include "Common.h"

class ThreadCache {
public:
	void* Allocate(size_t size);
	void Deallocate(void* ptr, size_t size);

	//����һ��central cache��ȡ�ڴ�
	void* FetchFromCentralCache(size_t index, size_t size);
	// �ͷŶ���ʱ����������������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _freeLists[NFREE_LIST];
};

static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;