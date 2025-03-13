#define _CRT_SECURE_NO_WARNINGS 1

#include"ConcurrentAlloc.h"

// ntimes һ��������ͷ��ڴ�Ĵ���
// rounds �ִ�
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds) {
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime(0);
	std::atomic<size_t> free_costtime(0);

	for (size_t k = 0; k < nworks; ++k) {
		vthread[k] = std::thread([&, k]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			size_t local_malloc_costtime = 0;
			size_t local_free_costtime = 0;

			for (size_t j = 0; j < rounds; ++j) {
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++) {
					v.push_back(malloc(16));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++) {
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				local_malloc_costtime += (end1 - begin1) * 1000 / CLOCKS_PER_SEC;
				local_free_costtime += (end2 - begin2) * 1000 / CLOCKS_PER_SEC;
			}

			malloc_costtime += local_malloc_costtime;
			free_costtime += local_free_costtime;
			});
	}

	for (auto& t : vthread) {
		t.join();
	}

	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�malloc "
		<< ntimes << "��: ���ѣ�" << malloc_costtime.load() << " ms\n";

	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�free "
		<< ntimes << "��: ���ѣ�" << free_costtime.load() << " ms\n";

	std::cout << nworks << "���̲߳���malloc&free " << nworks * rounds * ntimes
		<< "�Σ��ܼƻ��ѣ�" << (malloc_costtime.load() + free_costtime.load()) << " ms\n";
}


// ���ִ������ͷŴ��� �߳��� �ִ�
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	std::atomic<size_t> malloc_costtime = 0;
	std::atomic<size_t> free_costtime = 0;

	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);

			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(16));
					//v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
				}
				size_t end1 = clock();

				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();

				malloc_costtime += (end1 - begin1);
				free_costtime += (end2 - begin2);
			}
			});
	}

	for (auto& t : vthread)
	{
		t.join();
	}

	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�malloc "
		<< ntimes << "��: ���ѣ�" << malloc_costtime.load() << " ms\n";

	std::cout << nworks << "���̲߳���ִ��" << rounds << "�ִΣ�ÿ�ִ�free "
		<< ntimes << "��: ���ѣ�" << free_costtime.load() << " ms\n";

	std::cout << nworks << "���̲߳���malloc&free " << nworks * rounds * ntimes
		<< "�Σ��ܼƻ��ѣ�" << (malloc_costtime.load() + free_costtime.load()) << " ms\n";
}

int main()
{
	size_t n = 100000;
	cout << "==========================================================" << endl;
	BenchmarkConcurrentMalloc(n, 4, 10);
	cout << endl << endl;

	BenchmarkMalloc(n, 4, 10);
	cout << "==========================================================" << endl;

	return 0;
}