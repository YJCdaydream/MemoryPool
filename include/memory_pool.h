#pragma once

#include <atomic>  // 原子操作
#include <cassert> // 断言
#include <iostream>
#include <mutex> // 锁

namespace V1_memoryPool
{

#define MEMORY_POOL_NUM 64 // 内存池数量，hash的大小
#define SLOT_BASE_SIZE 8   // 闲置空间 槽， 最小的槽大小， 小于该大小则直接用该槽
#define MAX_SLOT_SIZE 521  // 最大的槽大小，超过该大小则直接使用malloc 来分配空间

    struct Slot
    { // 用于表示空间的槽
        std::atomic<Slot *> next;
    };

    class MemoryPool
    {
    private:
        int BlockSize_; // 内存块大小
        int SlotSize_;  // 槽大小

        Slot *firstBlock_; // 指向内存池管理的首个大块空间

        Slot *curSlot_;  // 指向当前未使用过的第一个空间，一分下来就是一大块，且使用的槽释放后都往前插，自然它会往后挤挤
        Slot *lastSlot_; //  作为当前未使用的空间的最后一个，超过该位置就需要重新分配空间

        // 更改为原子指针， 将变为原子操作
        std::atomic<Slot *> freeList_; // 指向 所有块 中空闲的槽（被使用后又被释放的槽，即一个空闲空间的队列（释放后将会头插法进入到该队列

        std::mutex mutexForFreeList_; // 保证多线程下操作空闲队列的原子性
        std::mutex mutexForBlock_;    // 保证多线程下避免重复开辟空间

    public:
        MemoryPool(size_t BlockSize = 4096);
        ~MemoryPool();

        void init(size_t);
        void *allocate();        //  重写类STL分配器- 如果闲置队列能满足，则直接分配，否则为其重新向系统请求空间
        void deallocate(void *); // 销毁空间 -不用了就把它挂到闲置空间队列里面去
    private:
        void allocateNewBlock(); // 不向外暴露的向系统请求空间
        size_t padPointer(char *p, size_t align);

        // 使用CAS操作进行无锁入队和出队
        bool pushFreeList(Slot *slot);
        Slot *popFreeList();
    };

    /*
     * 使用哈希映射的多种定长内存分配器（内存池中的一种
     * 每一个hash对应一个size 的 memory pool
     * 可以快速的分配对应空间
     */
    class HashBucket
    {
    public:
        static void initMemoryPool();
        static MemoryPool &getMemoryPool(int index);

        // 静态函数需要防止重复定义
        static void *useMemory(size_t size) // 按需分配空间
        {
            if (size < 0) // 空间为0 直接返回空指针
                return nullptr;
            if (size > MAX_SLOT_SIZE) // 超出最大空间，直接用new来分配
                return operator new(size);

            // 否则按需分配hash的对应槽, 相当于向上取整， 减一是因为hash的下标从0开始
            return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
        }

        static void freeMemory(void *ptr, size_t size) // 回收空间
        {
            if (!ptr)
                return;
            if (size > MAX_SLOT_SIZE)
            {
                operator delete(ptr);
                return;
            }
            getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
            return;
        }

        // 模板函数必须放置在头文件以便在编译时实例化
        template <typename T, typename... Args>
        friend T *newElement(Args &&...args);

        template <typename T>
        friend void deletElement(T *p);
    };

    // 按照对应的类型分配空间给T
    template <typename T, typename... Args>
    T *newElement(Args &&...args)
    {
        std::cout << "newElement开始分配内存" << std::endl;
        T *p = nullptr;
        // 根据函数大小选取合适的内存池分配内存
        if ((p = reinterpret_cast<T *>(HashBucket::useMemory(sizeof(T)))) != nullptr)
            new (p) T(std::forward<Args>(args)...);
        return p;
    }

    template <typename T>
    void deletElement(T *p)
    {
        if (p)
        {
            p->~T(); // 调用类型自己的析构函数，释放掉自己的含义
            // 内存回收
            HashBucket::freeMemory(reinterpret_cast<void *>(p), sizeof(T));
        }
    }
}
