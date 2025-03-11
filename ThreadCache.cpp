#define _CRT_SECURE_NO_WARNINGS 1
#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size) {
	assert(size <= MAX_BYTES);
	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].Empty()) {
		// 从threadcache获取内存
		return _freeLists[index].Pop();
	}
	else {
		// 从下一层，central cache获取内存
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size) {
	assert(size <= MAX_BYTES);
	assert(ptr);

	//找出对应映射的自由链表桶，对象插进去
	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(ptr);
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size) {
	// 慢开始反馈调节算法
	// 小对象给多点，对象给少点
	// 1、最开始一次不会向central cache要太多，要多了可能用不完；
	//	2、如果不要这个size大小内存需求，那么batchNum会不断增长直到上限
	// 3、size越大，一次向central cache要的batchNum就越小
	// 4、size越小，一次向central cache要的batchNum就越大
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(size));
	if (_freeLists[index].MaxSize() == batchNum) {
		_freeLists[index].MaxSize() += 1;
	}

	void* start = nullptr, * end = nullptr;
	//实际上至少得有一个
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