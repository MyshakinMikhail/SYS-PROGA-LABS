    #include <not_implemented.h>
    #include "../include/allocator_global_heap.h"
    #include <cstring>

    allocator_global_heap::allocator_global_heap()
    {
    }

    [[nodiscard]] void *allocator_global_heap::do_allocate_sm(
        size_t size)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        
        size_t total_size = size_t_size + size;
        
        auto memory_block = reinterpret_cast<unsigned char*>(::operator new(total_size));
        
        *reinterpret_cast<size_t*>(memory_block) = size;
        
        return memory_block + size_t_size;
    }

    void allocator_global_heap::do_deallocate_sm(
        void *at)
    {
        if (at == nullptr)
        {
            return;
        }
        
        std::lock_guard<std::mutex> lock(_mutex);
        
        auto memory_block = reinterpret_cast<unsigned char*>(at) - size_t_size;
        
        ::operator delete(memory_block);
    }

    allocator_global_heap::~allocator_global_heap()
    {
    }

    allocator_global_heap::allocator_global_heap(const allocator_global_heap &other)
    {
    }

    allocator_global_heap &allocator_global_heap::operator=(const allocator_global_heap &other)
    {
        if (this != &other)
        {
        }
        return *this;
    }

    bool allocator_global_heap::do_is_equal(const std::pmr::memory_resource &other) const noexcept
    {
        return dynamic_cast<const allocator_global_heap*>(&other) != nullptr;
    }

    allocator_global_heap::allocator_global_heap(allocator_global_heap &&other) noexcept
    {
    }

    allocator_global_heap &allocator_global_heap::operator=(allocator_global_heap &&other) noexcept
    {
        if (this != &other)
        {
        }
        return *this;
    }
