#define _CRT_SECURE_NO_WARNINGS 1

#include <vector>
#include <ctime>
#include "ConCurrentAlloc.h"

struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;
	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{
	// 申请释放的轮次 
	const size_t Rounds = 3;
	// 每轮申请释放多少次 
	const size_t N = 10000000;
	size_t begin1 = clock();
	std::vector<TreeNode*> v1;
	v1.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}
	size_t end1 = clock();
	ObjectPool<TreeNode> TNPool;
	size_t begin2 = clock();
	std::vector<TreeNode*> v2;
	v2.reserve(N);
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < N; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t end2 = clock();
	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "object pool cost time:" << end2 - begin2 << endl;
}
void Alloc1() {
	for (int i = 0; i < 5; i++) {
		void* ptr = ConcurrentAlloc(6);
	}
}
void Alloc2() {
	for (int i = 0; i < 5; i++) {
		void* ptr = ConcurrentAlloc(7);
	}
}
void test_TSL() {
	std::thread t1(Alloc1);
	std::thread t2(Alloc2);

	t1.join();
	t2.join();
}

void test_ConcurrentAlloc1() {
	void* p1 = ConcurrentAlloc(6);
	void* p2 = ConcurrentAlloc(8);
	void* p3 = ConcurrentAlloc(1);
	void* p4 = ConcurrentAlloc(7);
	void* p5 = ConcurrentAlloc(8);

	ConcurrentFree(p1);
	ConcurrentFree(p2);
	ConcurrentFree(p3);
	ConcurrentFree(p4);
	ConcurrentFree(p5);
}

void test_ConcurrentAlloc2() {
	for (int i = 0; i < 5; i++) {
		void* p1 = ConcurrentAlloc(6);
		cout << p1 << endl;
	}

	void* p2 = ConcurrentAlloc(8);
	cout << p2 << endl;
}

void ThreadAlloc1() {
	std::vector<void*> v;
	for (int i = 0; i < 7; i++) {
		void* ptr = ConcurrentAlloc(6);
		v.push_back(ptr);
	}

	for (auto& e : v) {
		ConcurrentFree(e);
	}
}
void ThreadAlloc2() {
	std::vector<void*> v;
	for (int i = 0; i < 7; i++) {
		void* ptr = ConcurrentAlloc(16);
		v.push_back(ptr);
	}

	for (auto& e : v) {
		ConcurrentFree(e);
	}
}

void test_MutilThreadAlloc() {
	std::thread t1(ThreadAlloc1);
	std::thread t2(ThreadAlloc2);
	t1.join();
	t2.join();
}

void BigAlloc() {
	// 大内存申请
	void* p1 = ConcurrentAlloc(257 * 1024);
	ConcurrentFree(p1);

	void* p2 = ConcurrentAlloc(129 * 8 * 1024);
	ConcurrentFree(p2);
}
//int main()
//{
//	//TestObjectPool();
//	//test_TSL();
//	test_ConcurrentAlloc1();
//	//test_MutilThreadAlloc();
//	//BigAlloc();
//	return 0;
//}