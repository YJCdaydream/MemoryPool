#include "../include/memory_pool.h"

namespace V1_memoryPool
{
    MemoryPool::MemoryPool(size_t BlockSize)
        : BlockSize_(BlockSize), SlotSize_(0), firstBlock_(nullptr), curSlot_(nullptr), freeList_(nullptr), lastSlot_(nullptr)
    {
    }

    MemoryPool::~MemoryPool()
    {
        // 把对应的分区全部删掉
        Slot *cur = firstBlock_;
        while (cur)
        {
            Slot *next = cur->next;
            // 将具体类型的内存都转成void*类型，则可以直接释放掉空间就行，无需析构函数
            operator delete(reinterpret_cast<void *>(cur));
            cur = next;
        }
    }

    void MemoryPool::init(size_t size)
    {
        assert(size > 0);
        SlotSize_ = size;
        firstBlock_ = nullptr;
        curSlot_ = nullptr;
        freeList_ = nullptr;
        lastSlot_ = nullptr;
    }

    void *MemoryPool::allocate()
    { //  重写类STL分配器- 如果闲置队列能满足，则直接分配，否则为其重新向系统请求空间
        // 首先询问已使用过的空闲槽
        std::cout << "MemoryPool allocated" << std::endl;
        Slot *slot = popFreeList(); // 弹出一个free节点
        if (slot != nullptr)
        {
            return slot;
        }

        // 如果空闲链表为空，则分配新内存

        Slot *temp;
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        // 查看未使用的空间
        if (curSlot_ >= lastSlot_)
        {
            // 无了，分配新的空间
            allocateNewBlock();
        }

        // 使用未使用的空间
        temp = curSlot_;
        curSlot_ += SlotSize_ / sizeof(Slot);
        return temp;
    }

    void MemoryPool::deallocate(void *ptr)
    {
        if (!ptr)
            return;

        // 回收空间， 头插法， 插入到闲置空间
        Slot *slot = reinterpret_cast<Slot *>(ptr); // 转成slot*类型，以便使用push函数插入
        pushFreeList(slot);
    }

    /* ----------------------不懂--------------------------------------*/
    void MemoryPool::allocateNewBlock()
    { // 不向外暴露的向系统请求空间
        // 申请整个一大块，然后切分成对应的slot_size块
        std::cout << "allocated 分配新块" << std::endl;
        void *newBlock = operator new(BlockSize_);
        reinterpret_cast<Slot *>(newBlock)->next = firstBlock_;
        firstBlock_ = reinterpret_cast<Slot *>(newBlock);

        char *body = reinterpret_cast<char *>(newBlock) + sizeof(Slot *);
        size_t paddingSize = padPointer(body, SlotSize_); // 计算对齐需要填充内存的大小
        curSlot_ = reinterpret_cast<Slot *>(body + paddingSize);

        // 超过该标记，则说明该内存块已无内存槽可用，需向系统申请新的内存块
        lastSlot_ = reinterpret_cast<Slot *>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);

        freeList_ = nullptr;
    }

    // 让指针对其到槽大小的倍数位置
    size_t MemoryPool::padPointer(char *p, size_t align)
    {
        return (align - reinterpret_cast<size_t>(p)) % align;
    }

    // 实现无锁入队操作
    bool MemoryPool::pushFreeList(Slot *slot)
    {
        while (true)
        {
            // 获取当前头结点
            Slot *oldHead = freeList_.load(std::memory_order_relaxed);
            // 将当前头结点的next 指向当前头结点
            slot->next.store(oldHead, std::memory_order_relaxed);

            // 尝试将新节点设置为头节点,交换
            if (freeList_.compare_exchange_weak(oldHead, slot,
                                                std::memory_order_release,
                                                std::memory_order_relaxed))
            {
                return true;
            }
        }
    }

    Slot *MemoryPool::popFreeList()
    {
        while (true)
        {
            // 获取要删除的节点
            Slot *oldHead = freeList_.load(std::memory_order_relaxed);

            if (oldHead == nullptr)
                return nullptr; // 链条为空

            // 获取下一个节点 ，访问newHead之前再次验证oldHead的有效性
            Slot *newHead;
            try
            {
                newHead = oldHead->next.load(std::memory_order_relaxed);
            }
            catch (...)
            {
                // 如果返回失败，则continue重试申请内存
                continue;
            }

            if (freeList_.compare_exchange_weak(oldHead, newHead,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed))
            {
                return oldHead;
            }
        }
    }

    /* ----------------------------HashBucket------------------------------*/
    void HashBucket::initMemoryPool()
    {
        for (int i = 0; i < MEMORY_POOL_NUM; i++)
        {
            getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
        }
    }

    // 单例模式
    MemoryPool &HashBucket::getMemoryPool(int index)
    {
        static MemoryPool memoryPool[MEMORY_POOL_NUM];
        return memoryPool[index];
    }
}