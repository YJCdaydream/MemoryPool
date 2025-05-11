#include "../include/memory_pool.h"

namespace V1_memoryPool
{
MemoryPool::MemoryPool(size_t BlockSize)
: BlockSize_ (BlockSize)
, SlotSize_ (0)
, firstBlock_ (nullptr)
, curSlot_ (nullptr)
, freeList_ (nullptr)
, lastSlot_ (nullptr)
{}

MemoryPool::~MemoryPool(){
    // 把对应的分区全部删掉
    Slot* cur = firstBlock_;
    while(cur)
    {
        Slot* next = cur->next;
        // 将具体类型的内存都转成void*类型，则可以直接释放掉空间就行，无需析构函数
        operator delete(reinterpret_cast<void*>(cur));  
        cur = next;
    }
}

void MemoryPool::init(size_t size){
    assert(size > 0);
    SlotSize_  = size;
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate(){ //  重写类STL分配器- 如果闲置队列能满足，则直接分配，否则为其重新向系统请求空间
    // 首先询问已使用过的空闲槽
    if(freeList_ != nullptr)
    {
        {
            // 操作空闲队列上锁
            std::lock_guard<std::mutex> lock(mutexForFreeList_);
            // 再次查询，防止死锁出现
            if(freeList_ != nullptr)
            {
                Slot* temp = freeList_;
                freeList_ = freeList_->next;
                return temp;
            }
        }
    }
    Slot* temp;
    {
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        // 查看未使用的空间
        if (curSlot_ >= lastSlot_)  
        {   
            // 无了，分配新的空间
            allocateNewBlock();
        }
        // 使用未使用的空间
        temp = curSlot_;
        curSlot_ = curSlot_->next;
        return temp;
    }
}

void MemoryPool::deallocate(void * ptr){
    if(ptr)
    {
        // 回收空间， 头插法， 插入到闲置空间
        std::lock_guard<std::mutex> lock(mutexForFreeList_);
        reinterpret_cast<Slot*>(ptr)->next = freeList_;
        freeList_ = reinterpret_cast<Slot*>(ptr);
    }
}    


/* ----------------------不懂--------------------------------------*/
void MemoryPool::allocateNewBlock(){// 不向外暴露的向系统请求空间
    // 申请整个一大块，然后切分成对应的slot_size块
    void* newBlock = operator new(BlockSize_);
    reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    size_t paddingSize = padPointer(body, SlotSize_);   // 计算对齐需要填充内存的大小
    curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

    // 超过该标记，则说明该内存块已无内存槽可用，需向系统申请新的内存块
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);

    freeList_ = nullptr;
}    

// 让指针对其到槽大小的倍数位置
size_t MemoryPool::padPointer(char * p, size_t align){
    return (align - reinterpret_cast<size_t>(p)) % align;
}

/* ----------------------------HashBucket------------------------------*/
void HashBucket::initMemoryPool(){
    for(int i = 0; i < MEMORY_POOL_NUM; i++)
    {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}


// 单例模式
MemoryPool& HashBucket::getMemoryPool(int index){
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    return memoryPool[index];
}
}