#define _CRT_SECURE_NO_WARNINGS 1
#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size) {
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].Empty()) {
		// ��threadcache��ȡ�ڴ�
		return _freeLists[index].Pop();
	}
	else {
		// ����һ�㣬central cache��ȡ�ڴ�
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(size <= MAX_BYTES);
	assert(ptr);

	//�ҳ���Ӧӳ�����������Ͱ��������ȥ
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	// ����ʼ���������㷨
	// С�������㣬������ٵ�
	// 1���ʼһ�β�����central cacheҪ̫�࣬Ҫ���˿����ò��ꣻ
	//	2�������Ҫ���size��С�ڴ�������ôbatchNum�᲻������ֱ������
	// 3��sizeԽ��һ����central cacheҪ��batchNum��ԽС
	// 4��sizeԽС��һ����central cacheҪ��batchNum��Խ��
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchNum) {
		_freeLists[index].MaxSize() += 1;
	}

	void* start = nullptr, * end = nullptr;
	//ʵ�������ٵ���һ��
	auto actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);

	if (actualNum == 1) {
		assert(start == end);
	}
	else {
		_freeLists[index].PushRange(NextObj(start), end);
	}
	return start;
}