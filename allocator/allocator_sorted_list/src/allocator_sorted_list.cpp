#include <not_implemented.h>
#include <cstring>
#include "../include/allocator_sorted_list.h"

allocator_sorted_list::~allocator_sorted_list()
{
    if (_trusted_memory == nullptr) return;
    
    std::pmr::memory_resource* parent = *reinterpret_cast<std::pmr::memory_resource**>(_trusted_memory);
    size_t total_size = *reinterpret_cast<size_t*>(
        static_cast<char*>(_trusted_memory)
        + sizeof(std::pmr::memory_resource*)
        + sizeof(allocator_with_fit_mode::fit_mode));

    if (parent != nullptr) {
        parent->deallocate(_trusted_memory, total_size);
    } else {
        ::operator delete(_trusted_memory);
    }
    _trusted_memory = nullptr;
}

allocator_sorted_list::allocator_sorted_list(
    allocator_sorted_list &&other) noexcept
{
    if (this != &other) {
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
}

allocator_sorted_list &allocator_sorted_list::operator=(
    allocator_sorted_list &&other) noexcept
{
    if (this != &other) {
        this->~allocator_sorted_list();
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    return *this;
}

allocator_sorted_list::allocator_sorted_list(
        size_t space_size,
        std::pmr::memory_resource *parent_allocator,
        allocator_with_fit_mode::fit_mode allocate_fit_mode)
{   
    if (space_size < block_metadata_size) {
        throw std::bad_alloc();
    }
    
    size_t total_size = allocator_metadata_size + space_size;
    
    void* memory;
    if (parent_allocator != nullptr) {
        memory = parent_allocator->allocate(total_size);
    } else {
        memory = ::operator new(total_size);
    }
    
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    
    _trusted_memory = memory;
    
    char* ptr = static_cast<char*>(_trusted_memory);
    
    *reinterpret_cast<std::pmr::memory_resource**>(ptr) = parent_allocator;
    ptr += sizeof(std::pmr::memory_resource*);
    
    *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(ptr) = allocate_fit_mode;
    ptr += sizeof(allocator_with_fit_mode::fit_mode);
    
    *reinterpret_cast<size_t*>(ptr) = total_size;
    ptr += sizeof(size_t);
    
    new (ptr) std::mutex();
    ptr += sizeof(std::mutex);
    
    *reinterpret_cast<void**>(ptr) = static_cast<char*>(_trusted_memory) + allocator_metadata_size;
    ptr += sizeof(void*);
    
    void* first_block = static_cast<char*>(_trusted_memory) + allocator_metadata_size;
    *reinterpret_cast<void**>(first_block) = nullptr;
    *reinterpret_cast<size_t*>(static_cast<char*>(first_block) + sizeof(void*)) = space_size;
}

[[nodiscard]] void *allocator_sorted_list::do_allocate_sm(
    size_t size)
{
    if (_trusted_memory == nullptr) throw std::bad_alloc();
    
    char* metadata_ptr = static_cast<char*>(_trusted_memory);
    std::mutex& mtx = *reinterpret_cast<std::mutex*>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode) + 
        sizeof(size_t));
    
    std::lock_guard<std::mutex> lock(mtx);
    
    allocator_with_fit_mode::fit_mode current_mode = *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(
        metadata_ptr + sizeof(std::pmr::memory_resource*));
    
    void*& free_head = *reinterpret_cast<void**>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode) + 
        sizeof(size_t) + 
        sizeof(std::mutex));
    
    if (size > SIZE_MAX - block_metadata_size) {
        throw std::bad_alloc();
    }
    
    size_t aligned_size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);
    size_t total_required = aligned_size + block_metadata_size;
    
    void* best_block = nullptr;
    void* prev_best = nullptr;
    void* current = free_head;
    void* prev = nullptr;
    size_t best_size = 0;
    
    while (current != nullptr) {
        size_t block_size = *reinterpret_cast<size_t*>(static_cast<char*>(current) + sizeof(void*));
        
        if (block_size >= total_required) {
            switch (current_mode) {
                case allocator_with_fit_mode::fit_mode::first_fit:
                    best_block = current;
                    prev_best = prev;
                    current = nullptr;
                    continue;
                    
                case allocator_with_fit_mode::fit_mode::the_best_fit:
                    if (best_block == nullptr || block_size < best_size) {
                        best_block = current;
                        prev_best = prev;
                        best_size = block_size;
                    }
                    break;
                    
                case allocator_with_fit_mode::fit_mode::the_worst_fit:
                    if (best_block == nullptr || block_size > best_size) {
                        best_block = current;
                        prev_best = prev;
                        best_size = block_size;
                    }
                    break;
            }
        }
        
        prev = current;
        current = *reinterpret_cast<void**>(current);
    }
    
    if (best_block == nullptr) {
        throw std::bad_alloc();
    }
    
    size_t best_block_size = *reinterpret_cast<size_t*>(static_cast<char*>(best_block) + sizeof(void*));
    
    if (best_block_size - total_required >= block_metadata_size) {
        void* new_block = static_cast<char*>(best_block) + total_required;
        size_t remaining_size = best_block_size - total_required;
        
        *reinterpret_cast<void**>(new_block) = *reinterpret_cast<void**>(best_block);
        *reinterpret_cast<size_t*>(static_cast<char*>(new_block) + sizeof(void*)) = remaining_size;
        
        if (prev_best == nullptr) {
            free_head = new_block;
        } else {
            *reinterpret_cast<void**>(prev_best) = new_block;
        }
        
        *reinterpret_cast<void**>(best_block) = _trusted_memory;
        *reinterpret_cast<size_t*>(static_cast<char*>(best_block) + sizeof(void*)) = total_required;
    } else {
        if (prev_best == nullptr) {
            free_head = *reinterpret_cast<void**>(best_block);
        } else {
            *reinterpret_cast<void**>(prev_best) = *reinterpret_cast<void**>(best_block);
        }
        
        *reinterpret_cast<void**>(best_block) = _trusted_memory;
    }
    
    return static_cast<char*>(best_block) + block_metadata_size;
}

allocator_sorted_list::allocator_sorted_list(const allocator_sorted_list &other)
{
    if (other._trusted_memory == nullptr) {
        _trusted_memory = nullptr;
        return;
    }
    
    char* other_metadata = static_cast<char*>(other._trusted_memory);
    
    std::pmr::memory_resource* parent = *reinterpret_cast<std::pmr::memory_resource**>(other_metadata);
    size_t total_size = *reinterpret_cast<size_t*>(
        other_metadata + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode));
    
    void* memory;
    if (parent != nullptr) {
        memory = parent->allocate(total_size);
    } else {
        memory = ::operator new(total_size);
    }
    
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    
    _trusted_memory = memory;
    
    std::memcpy(_trusted_memory, other._trusted_memory, total_size);
    
    char* ptr = static_cast<char*>(_trusted_memory);
    ptr += sizeof(std::pmr::memory_resource*) + 
           sizeof(allocator_with_fit_mode::fit_mode) + 
           sizeof(size_t);
    
    reinterpret_cast<std::mutex*>(ptr)->~mutex();
    new (ptr) std::mutex();
}

allocator_sorted_list &allocator_sorted_list::operator=(const allocator_sorted_list &other)
{
    if (this == &other) {
        return *this;
    }
    
    if (_trusted_memory != nullptr) {
        char* metadata_ptr = static_cast<char*>(_trusted_memory);
        std::pmr::memory_resource* parent = *reinterpret_cast<std::pmr::memory_resource**>(metadata_ptr);
        size_t total_size = *reinterpret_cast<size_t*>(
            metadata_ptr + sizeof(std::pmr::memory_resource*) + 
            sizeof(allocator_with_fit_mode::fit_mode));
        
        if (parent != nullptr) {
            parent->deallocate(_trusted_memory, total_size);
        } else {
            ::operator delete(_trusted_memory);
        }
    }
    
    if (other._trusted_memory == nullptr) {
        _trusted_memory = nullptr;
        return *this;
    }
    
    char* other_metadata = static_cast<char*>(other._trusted_memory);
    std::pmr::memory_resource* parent = *reinterpret_cast<std::pmr::memory_resource**>(other_metadata);
    size_t total_size = *reinterpret_cast<size_t*>(
        other_metadata + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode));
    
    void* memory;
    if (parent != nullptr) {
        memory = parent->allocate(total_size);
    } else {
        memory = ::operator new(total_size);
    }
    
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    
    _trusted_memory = memory;
    std::memcpy(_trusted_memory, other._trusted_memory, total_size);
    
    // Переинициализируем мьютекс
    char* ptr = static_cast<char*>(_trusted_memory);
    ptr += sizeof(std::pmr::memory_resource*) + 
           sizeof(allocator_with_fit_mode::fit_mode) + 
           sizeof(size_t);
    
    reinterpret_cast<std::mutex*>(ptr)->~mutex();
    new (ptr) std::mutex();
    
    return *this;
}

bool allocator_sorted_list::do_is_equal(const std::pmr::memory_resource &other) const noexcept
{
    return this == &other;
}

void allocator_sorted_list::do_deallocate_sm(
    void *at)
{
    if (at == nullptr || _trusted_memory == nullptr) return;
    
    char* metadata_ptr = static_cast<char*>(_trusted_memory);
    std::mutex& mtx = *reinterpret_cast<std::mutex*>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode) + 
        sizeof(size_t));
    
    std::lock_guard<std::mutex> lock(mtx);
    
    void* block = static_cast<char*>(at) - block_metadata_size;
    
    size_t total_size = *reinterpret_cast<size_t*>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode));
    
    if (block < static_cast<char*>(_trusted_memory) + 
        (sizeof(std::pmr::memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode) + 
         sizeof(size_t) + sizeof(std::mutex) + sizeof(void*)) ||
        block >= static_cast<char*>(_trusted_memory) + total_size) {
        return;
    }

    if (*reinterpret_cast<void**>(block) != _trusted_memory) {
        return;
    }
    
    size_t block_size = *reinterpret_cast<size_t*>(static_cast<char*>(block) + sizeof(void*));
    
    *reinterpret_cast<void**>(block) = nullptr;
    
    void*& free_head = *reinterpret_cast<void**>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode) + 
        sizeof(size_t) + 
        sizeof(std::mutex));
    
    void* current = free_head;
    void* prev = nullptr;
    
    while (current != nullptr && current < block) {
        prev = current;
        current = *reinterpret_cast<void**>(current);
    }
    
    *reinterpret_cast<void**>(block) = current;
    if (prev == nullptr) {
        free_head = block;
    } else {
        *reinterpret_cast<void**>(prev) = block;
    }
    
    if (current != nullptr && 
        static_cast<char*>(block) + block_size == static_cast<char*>(current)) {
        block_size += *reinterpret_cast<size_t*>(static_cast<char*>(current) + sizeof(void*));
        *reinterpret_cast<void**>(block) = *reinterpret_cast<void**>(current);
        *reinterpret_cast<size_t*>(static_cast<char*>(block) + sizeof(void*)) = block_size;
    }
    
    if (prev != nullptr && 
        static_cast<char*>(prev) + *reinterpret_cast<size_t*>(static_cast<char*>(prev) + sizeof(void*)) == static_cast<char*>(block)) {
        size_t prev_size = *reinterpret_cast<size_t*>(static_cast<char*>(prev) + sizeof(void*));
        *reinterpret_cast<size_t*>(static_cast<char*>(prev) + sizeof(void*)) = prev_size + block_size;
        *reinterpret_cast<void**>(prev) = *reinterpret_cast<void**>(block);
    }
    // должен ли аллокатор стирать метаданные старого слитого блока
}

inline void allocator_sorted_list::set_fit_mode(
    allocator_with_fit_mode::fit_mode mode)
{
    if (_trusted_memory == nullptr) return;
    
    char* metadata_ptr = static_cast<char*>(_trusted_memory);
    std::mutex& mtx = *reinterpret_cast<std::mutex*>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode) + 
        sizeof(size_t));
    
    std::lock_guard<std::mutex> lock(mtx);
    
    *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(
        metadata_ptr + sizeof(std::pmr::memory_resource*)) = mode;
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info() const noexcept
{
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_sorted_list::get_blocks_info_inner() const
{
    std::vector<allocator_test_utils::block_info> result;
    
    if (_trusted_memory == nullptr) return result;

    for (auto it = begin(); it != end(); ++it) {
        result.push_back(allocator_test_utils::block_info{
            it.size(),
            it.occupied()
        });
    }
    
    return result;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_begin() const noexcept
{
    if (_trusted_memory == nullptr) return sorted_free_iterator(nullptr);
    
    char* metadata_ptr = static_cast<char*>(_trusted_memory);
    void* free_head = *reinterpret_cast<void**>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode) + 
        sizeof(size_t) + 
        sizeof(std::mutex));
    
    return sorted_free_iterator(free_head);
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::free_end() const noexcept
{
    return sorted_free_iterator(nullptr);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::begin() const noexcept
{
    return sorted_iterator(_trusted_memory);
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::end() const noexcept
{
    return sorted_iterator(nullptr);
}

bool allocator_sorted_list::sorted_free_iterator::operator==(
        const allocator_sorted_list::sorted_free_iterator & other) const noexcept
{
    return _free_ptr == other._free_ptr;
}

bool allocator_sorted_list::sorted_free_iterator::operator!=(
        const allocator_sorted_list::sorted_free_iterator &other) const noexcept
{
    return _free_ptr != other._free_ptr;
}

allocator_sorted_list::sorted_free_iterator &allocator_sorted_list::sorted_free_iterator::operator++() & noexcept
{
    if (_free_ptr != nullptr) {
        _free_ptr = *reinterpret_cast<void**>(_free_ptr);
    }
    return *this;
}

allocator_sorted_list::sorted_free_iterator allocator_sorted_list::sorted_free_iterator::operator++(int n)
{
    sorted_free_iterator tmp = *this;
    ++(*this);
    return tmp;
}

size_t allocator_sorted_list::sorted_free_iterator::size() const noexcept
{
    if (_free_ptr == nullptr) return 0;
    return *reinterpret_cast<size_t*>(static_cast<char*>(_free_ptr) + sizeof(void*));
}

void *allocator_sorted_list::sorted_free_iterator::operator*() const noexcept
{
    if (_free_ptr == nullptr) return nullptr;
    return static_cast<char*>(_free_ptr) + block_metadata_size;
}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator() : _free_ptr(nullptr) {}

allocator_sorted_list::sorted_free_iterator::sorted_free_iterator(void *trusted) : _free_ptr(trusted) {}

bool allocator_sorted_list::sorted_iterator::operator==(const allocator_sorted_list::sorted_iterator & other) const noexcept
{
    return _current_ptr == other._current_ptr;
}

bool allocator_sorted_list::sorted_iterator::operator!=(const allocator_sorted_list::sorted_iterator &other) const noexcept
{
    return _current_ptr != other._current_ptr;
}

allocator_sorted_list::sorted_iterator &allocator_sorted_list::sorted_iterator::operator++() & noexcept
{
    if (_current_ptr != nullptr && _trusted_memory != nullptr) {
        size_t block_size = *reinterpret_cast<size_t*>(static_cast<char*>(_current_ptr) + sizeof(void*));
        char* next_ptr = static_cast<char*>(_current_ptr) + block_size;
        
        size_t total_size = *reinterpret_cast<size_t*>(
            static_cast<char*>(_trusted_memory) + sizeof(std::pmr::memory_resource*) + 
            sizeof(allocator_with_fit_mode::fit_mode));
        
        if (next_ptr >= static_cast<char*>(_trusted_memory) + total_size) {
            _current_ptr = nullptr;
        } else {
            _current_ptr = next_ptr;
        }
    }
    return *this;
}

allocator_sorted_list::sorted_iterator allocator_sorted_list::sorted_iterator::operator++(int n)
{
    sorted_iterator tmp = *this;
    ++(*this);
    return tmp;
}

size_t allocator_sorted_list::sorted_iterator::size() const noexcept
{
    if (_current_ptr == nullptr) return 0;
    return *reinterpret_cast<size_t*>(static_cast<char*>(_current_ptr) + sizeof(void*));
}

void *allocator_sorted_list::sorted_iterator::operator*() const noexcept
{
    if (_current_ptr == nullptr) return nullptr;
    return static_cast<char*>(_current_ptr) + block_metadata_size;
}

allocator_sorted_list::sorted_iterator::sorted_iterator() : _current_ptr(nullptr), _trusted_memory(nullptr) {}

allocator_sorted_list::sorted_iterator::sorted_iterator(void *trusted) : _trusted_memory(trusted)
{
    if (trusted != nullptr) {
        _current_ptr = static_cast<char*>(trusted) + allocator_metadata_size;
    } else {
        _current_ptr = nullptr;
    }
}

bool allocator_sorted_list::sorted_iterator::occupied() const noexcept
{
    if (_current_ptr == nullptr || _trusted_memory == nullptr) return false;
    char* metadata_ptr = static_cast<char*>(_trusted_memory);
    void* free_head = *reinterpret_cast<void**>(
        metadata_ptr + sizeof(std::pmr::memory_resource*) + 
        sizeof(allocator_with_fit_mode::fit_mode) + 
        sizeof(size_t) + 
        sizeof(std::mutex));

    void* current = free_head;
    while (current != nullptr) {
        if (current == _current_ptr) {
            return false;
        }
        current = *reinterpret_cast<void**>(current);
    }
    
    return true;
}