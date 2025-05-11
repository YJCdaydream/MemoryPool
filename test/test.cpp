#include <iostream>
#include <thread>
#include <vector>
#include "../include/memory_pool.h"

// 创建不同容量的类来作为测试的材料
class test1
{
    int id_;    // 一个int
};

class test2
{
    int id_[5]; // 5个int
};

class test3
{
    int id_[10];
};

class test4
{
    int id_[20];
};

// 单轮次申请释放次数 线程数 轮次
void BenchmarkMemoryPool(size_t ntimes, size_t nworks, size_t rounds)
{
    std::vector<std::thread> vthread(nworks);   // 创建一个内存组
    size_t total_costtime = 0;
    for(size_t k = 0; k < nworks; ++k)  // 创建 n个works
    {
        vthread[k] = std::thread([&](){ 
            for(size_t r = 0; r < rounds; r++)   // 每个来 rounds 回 求总计时间
            {
                size_t begin1 = clock();
                for(size_t t = 0; t < ntimes; t++)  //
                {
                    // 来回创建并销毁对象
                    test1* tt1 = V1_memoryPool::newElement<test1>();
                    V1_memoryPool::deletElement(tt1);
                    
                    test2* tt2 = V1_memoryPool::newElement<test2>();
                    V1_memoryPool::deletElement(tt2);

                    test3* tt3 = V1_memoryPool::newElement<test3>();
                    V1_memoryPool::deletElement(tt3);

                    test4* tt4 = V1_memoryPool::newElement<test4>();
                    V1_memoryPool::deletElement(tt4);
                }
                size_t end1 = clock();

                total_costtime += end1 - begin1;
            }
        });
    }
    for(auto& t : vthread)
    {
        t.join();
    }
    printf("%lu个线程并发执行%lu轮次，每轮次newElement %lu次， 总计花费：%lu ms\n", nworks, rounds, ntimes, total_costtime);
}

void BenchmarkNew(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t total_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
                    // 对比试验，通过 new 
                    test1* tt1 = new test1;
                    delete tt1;
                    
                    test2* tt2 = new test2;
                    delete tt2;

                    test3* tt3 = new test3;
                    delete tt3;

                    test4* tt4 = new test4;
                    delete tt4;
				}
				size_t end1 = clock();
				
				total_costtime += end1 - begin1;
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%lu个线程并发执行%lu轮次，每轮次malloc&free %lu次，总计花费：%lu ms\n", nworks, rounds, ntimes, total_costtime);
}

int main()
{
    V1_memoryPool::HashBucket::initMemoryPool();    // 使用内存池接口前一定要先调用该函数
    BenchmarkMemoryPool(100, 1, 10);    // 测试内存池
    std::cout << "======================================" << std::endl;
    std::cout << "======================================" << std::endl;
    BenchmarkNew(100, 1, 10);   // 测试new
    return 0;
}