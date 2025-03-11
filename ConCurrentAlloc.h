#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

static void* ConcurrentAlloc(size_t size) {
	if (size > MAX_BYTES) {
		//大于256kb
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;//页数

		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = size;
		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
		return ptr;
	}

	//通过TLS 每个线程无锁的获取自己专属的ThreadCache对象
	if (pTLSThreadCache == nullptr) {
		static ObjectPool<ThreadCache> tcPool;
		//pTLSThreadCache = new ThreadCache;
		pTLSThreadCache = tcPool.New();
	}

	cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;
	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr) {
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if (size > MAX_BYTES) {
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else {
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr, size);
	}
}