#include "../include/allocator_boundary_tags.h"
#include <cstring>
#include <mutex>


namespace {
    constexpr const size_t ALLOCATOR_METADATA_SIZE = sizeof(std::pmr::memory_resource*) + 
                                                      sizeof(allocator_with_fit_mode::fit_mode) +
                                                       sizeof(size_t) + sizeof(std::mutex) + sizeof(void*);
    
    constexpr const size_t OCCUPIED_BLOCK_METADATA_SIZE = sizeof(size_t) + sizeof(void*) + sizeof(void*) + sizeof(void*);
    
    std::pmr::memory_resource*& get_parent_allocator(void* trusted) {
        return *reinterpret_cast<std::pmr::memory_resource**>(trusted);
    }
    
    allocator_with_fit_mode::fit_mode& get_fit_mode(void* trusted) {
        return *reinterpret_cast<allocator_with_fit_mode::fit_mode*>(
            static_cast<char*>(trusted) + sizeof(std::pmr::memory_resource*)
        );
    }
    
    size_t& get_trusted_size(void* trusted) {
        return *reinterpret_cast<size_t*>(
            static_cast<char*>(trusted) + sizeof(std::pmr::memory_resource*) + sizeof(allocator_with_fit_mode::fit_mode)
        );
    }
    
    std::mutex& get_mutex(void* trusted) {
        return *reinterpret_cast<std::mutex*>(
            static_cast<char*>(trusted) + sizeof(std::pmr::memory_resource*) + 
            sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t)
        );
    }
    
    void*& get_first_occupied(void* trusted) {
        return *reinterpret_cast<void**>(
            static_cast<char*>(trusted) + sizeof(std::pmr::memory_resource*) + 
            sizeof(allocator_with_fit_mode::fit_mode) + sizeof(size_t) + sizeof(std::mutex)
        );
    }
    
    size_t& get_block_size(void* block) {
        return *reinterpret_cast<size_t*>(block);
    }
    
    void*& get_prev_occupied(void* block) {
        return *reinterpret_cast<void**>(static_cast<char*>(block) + sizeof(size_t));
    }
    
    void*& get_next_occupied(void* block) {
        return *reinterpret_cast<void**>(static_cast<char*>(block) + sizeof(size_t) + sizeof(void*));
    }
    
    inline void*& get_block_owner(void* block) {
        return *reinterpret_cast<void**>(
            static_cast<char*>(block) + sizeof(size_t) + 2 * sizeof(void*)
        );
    }
        
    void* get_block_data(void* block) {
        return static_cast<char*>(block) + OCCUPIED_BLOCK_METADATA_SIZE;
    }
    
    void* get_next_block(void* block) {
        return static_cast<char*>(block) + get_block_size(block);
    }
    
    void* get_trusted_end(void* trusted) {
        return static_cast<char*>(trusted) + ALLOCATOR_METADATA_SIZE + get_trusted_size(trusted);
    }
    
    void* find_free_block(void* trusted, size_t required_size, allocator_with_fit_mode::fit_mode mode) {
        void* result = nullptr;
        size_t best_diff = (mode == allocator_with_fit_mode::fit_mode::the_worst_fit) ? 0 : SIZE_MAX;
        
        void* current = static_cast<char*>(trusted) + ALLOCATOR_METADATA_SIZE;
        void* end = get_trusted_end(trusted);
        void* first_occupied = get_first_occupied(trusted);
        
        while (current < end) {
            void* temp = first_occupied;
            while (temp && temp < current) {
                temp = get_next_occupied(temp);
            }

            
             if (temp == current) {
                current = get_next_block(current);
            } else {
                size_t free_size;
                if (temp) {
                    free_size = static_cast<char*>(temp) - static_cast<char*>(current);
                } else {
                    free_size = static_cast<char*>(end) - static_cast<char*>(current);
                }
                
                if (free_size >= required_size) {
                    size_t remaining = free_size - required_size;

                    if (remaining > 0 && remaining < OCCUPIED_BLOCK_METADATA_SIZE) {
                        if (mode == allocator_with_fit_mode::fit_mode::first_fit) {
                            return current;
                        }
                        remaining = 0;
                    }
                    
                    switch (mode) {
                        case allocator_with_fit_mode::fit_mode::first_fit:
                            return current;
                            
                        case allocator_with_fit_mode::fit_mode::the_best_fit:
                            if (remaining < best_diff) {
                                best_diff = remaining;
                                result = current;
                            }
                            break;
                            
                        case allocator_with_fit_mode::fit_mode::the_worst_fit:
                            if (remaining > best_diff) {
                                best_diff = remaining;
                                result = current;
                            }
                            break;
                    }
                }
                
                if (free_size > 0) {
                    current = static_cast<char*>(current) + free_size;
                } else {
                    break;
                }
            }
        }
        
        return result;
    }
    
    void insert_occupied_block(void* trusted, void* block) {
        void*& first = get_first_occupied(trusted);
        
        if (!first || block < first) {
            get_next_occupied(block) = first;
            get_prev_occupied(block) = nullptr;
            if (first) {
                get_prev_occupied(first) = block;
            }
            first = block;
            return;
        }

        void* current = first;
        while (get_next_occupied(current) && get_next_occupied(current) < block) {
            current = get_next_occupied(current);
        }
        
        void* next = get_next_occupied(current);
        get_next_occupied(block) = next;
        get_prev_occupied(block) = current;
        get_next_occupied(current) = block;
        if (next) {
            get_prev_occupied(next) = block;
        }
    }
    
    void remove_occupied_block(void* trusted, void* block) {
        void* prev = get_prev_occupied(block);
        void* next = get_next_occupied(block);
        
        if (prev) {
            get_next_occupied(prev) = next;
        } else {
            get_first_occupied(trusted) = next;
        }
        
        if (next) {
            get_prev_occupied(next) = prev;
        }
    }
    
    bool belongs_to_allocator(void* trusted, void* ptr) {
        if (!trusted || !ptr) return false;
        
        void* memory_start = static_cast<char*>(trusted) + ALLOCATOR_METADATA_SIZE;
        void* memory_end = get_trusted_end(trusted);
        
        return ptr >= memory_start && ptr < memory_end;
    }
    
    bool is_occupied_block_start(void* trusted, void* ptr) {
        if (!belongs_to_allocator(trusted, ptr)) return false;
        
        void* current = get_first_occupied(trusted);
        while (current) {
            if (current == ptr) return true;
            current = get_next_occupied(current);
        }
        return false;
    }
}

allocator_boundary_tags::~allocator_boundary_tags() {
    if (_trusted_memory == nullptr) return;
    
    std::pmr::memory_resource* parent = get_parent_allocator(_trusted_memory);
    size_t trusted_size = get_trusted_size(_trusted_memory);
    size_t total_size = trusted_size + allocator_metadata_size;
    
    get_mutex(_trusted_memory).~mutex();
    parent->deallocate(_trusted_memory, total_size);
}

allocator_boundary_tags::allocator_boundary_tags(const allocator_boundary_tags &other) {
    if (other._trusted_memory == nullptr) {
        _trusted_memory = nullptr;
        return;
    }
    
    std::lock_guard<std::mutex> lock(get_mutex(other._trusted_memory));
    
    size_t trusted_size = get_trusted_size(other._trusted_memory);
    size_t total_size = trusted_size + allocator_metadata_size;
    
    std::pmr::memory_resource* parent = get_parent_allocator(other._trusted_memory);
    void* memory = parent->allocate(total_size);
    
    if (!memory) {
        throw std::bad_alloc();
    }
    
    std::memcpy(memory, other._trusted_memory, total_size);
    _trusted_memory = memory;
    
    new (&get_mutex(_trusted_memory)) std::mutex();
    
    void* first_occupied = get_first_occupied(_trusted_memory);
    if (first_occupied) {
        ptrdiff_t offset = static_cast<char*>(_trusted_memory) - static_cast<char*>(other._trusted_memory);
        void* first_occupied = get_first_occupied(_trusted_memory);
        if (first_occupied) {
            first_occupied = static_cast<char*>(first_occupied) + offset;
            get_first_occupied(_trusted_memory) = first_occupied;
            void* current = first_occupied;
            while (current) {
                void* next = get_next_occupied(current);
                void* prev = get_prev_occupied(current);
                
                if (next) {
                    next = static_cast<char*>(next) + offset;
                    get_next_occupied(current) = next;
                }
                
                if (prev) {
                    prev = static_cast<char*>(prev) + offset;
                    get_prev_occupied(current) = prev;
                }
                current = next;
            }
        }
    }
}

allocator_boundary_tags &allocator_boundary_tags::operator=(const allocator_boundary_tags &other) {
    if (this != &other) {
        allocator_boundary_tags tmp(other);
        std::swap(_trusted_memory, tmp._trusted_memory);
    }
    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(allocator_boundary_tags &&other) noexcept {
    _trusted_memory = other._trusted_memory;
    other._trusted_memory = nullptr;
}

allocator_boundary_tags &allocator_boundary_tags::operator=(allocator_boundary_tags &&other) noexcept {
    if (this != &other) {
        if (_trusted_memory) {
            std::pmr::memory_resource* parent = get_parent_allocator(_trusted_memory);
            size_t trusted_size = get_trusted_size(_trusted_memory);
            size_t total_size = trusted_size + allocator_metadata_size;
            get_mutex(_trusted_memory).~mutex();
            parent->deallocate(_trusted_memory, total_size);
        }
        
        _trusted_memory = other._trusted_memory;
        other._trusted_memory = nullptr;
    }
    return *this;
}

allocator_boundary_tags::allocator_boundary_tags(
    size_t space_size,
    std::pmr::memory_resource *parent_allocator,
    allocator_with_fit_mode::fit_mode allocate_fit_mode) {
    
    if (space_size < occupied_block_metadata_size) {
        throw std::bad_alloc();
    }
    
    if (parent_allocator == nullptr) {
        parent_allocator = std::pmr::get_default_resource();
    }
    
    size_t total_size = allocator_metadata_size + space_size;
    void* memory = parent_allocator->allocate(total_size);
    
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    
    _trusted_memory = memory;
    
    get_parent_allocator(_trusted_memory) = parent_allocator;
    get_fit_mode(_trusted_memory) = allocate_fit_mode;
    get_trusted_size(_trusted_memory) = space_size;
    new (&get_mutex(_trusted_memory)) std::mutex();
    get_first_occupied(_trusted_memory) = nullptr;
}

[[nodiscard]] void *allocator_boundary_tags::do_allocate_sm(size_t size) {
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
    
    size_t total_block_size = size + OCCUPIED_BLOCK_METADATA_SIZE;
    
    allocator_with_fit_mode::fit_mode mode = get_fit_mode(_trusted_memory);
    void* free_block = find_free_block(_trusted_memory, total_block_size, mode);
    
    if (!free_block) {
        throw std::bad_alloc();
    }
    
    void* nearest_occupied = nullptr;
    void* temp = get_first_occupied(_trusted_memory);
    while (temp) {
        if (temp > free_block) {
            nearest_occupied = temp;
            break;
        }
        temp = get_next_occupied(temp);
    }
    
    size_t actual_free_size = 0;
    if (nearest_occupied) {
        actual_free_size = static_cast<char*>(nearest_occupied) - static_cast<char*>(free_block);
    } else {
        actual_free_size = static_cast<char*>(get_trusted_end(_trusted_memory)) - static_cast<char*>(free_block);
    }
    
    size_t remaining = actual_free_size - total_block_size;
    if (remaining > 0 && remaining < OCCUPIED_BLOCK_METADATA_SIZE) {
        total_block_size = actual_free_size;
    }
    
    get_block_size(free_block) = total_block_size;
    get_prev_occupied(free_block) = nullptr;
    get_next_occupied(free_block) = nullptr;
    get_block_owner(free_block) = _trusted_memory;
    insert_occupied_block(_trusted_memory, free_block);
    
    return get_block_data(free_block);
}

void allocator_boundary_tags::do_deallocate_sm(void *at) {
    if (!at) return;
    
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
    
    void* block = static_cast<char*>(at) - occupied_block_metadata_size;
    
    if (!is_occupied_block_start(_trusted_memory, block)) {
        return;
    }

    if (get_block_owner(block) != _trusted_memory) {
        return;
    }
    
    remove_occupied_block(_trusted_memory, block);
}

inline void allocator_boundary_tags::set_fit_mode(allocator_with_fit_mode::fit_mode mode) {
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
    get_fit_mode(_trusted_memory) = mode;
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info() const {
    std::lock_guard<std::mutex> lock(get_mutex(_trusted_memory));
    return get_blocks_info_inner();
}

std::vector<allocator_test_utils::block_info> allocator_boundary_tags::get_blocks_info_inner() const {
    std::vector<allocator_test_utils::block_info> result;
    
    void* current = static_cast<char*>(_trusted_memory) + allocator_metadata_size;
    void* end = get_trusted_end(_trusted_memory);
    
    while (current < end) {
        allocator_test_utils::block_info info;
        
        bool is_occupied = false;
        void* occupied_block = get_first_occupied(_trusted_memory);
        while (occupied_block) {
            if (occupied_block == current) {
                is_occupied = true;
                info.block_size = get_block_size(current);
                break;
            }
            occupied_block = get_next_occupied(occupied_block);
        }
        
        info.is_block_occupied = is_occupied;
        
        if (!is_occupied) {
            void* next_occupied = nullptr;
            void* temp = get_first_occupied(_trusted_memory);
            while (temp) {
                if (temp > current) {
                    next_occupied = temp;
                    break;
                }
                temp = get_next_occupied(temp);
            }
            
            if (next_occupied) {
                info.block_size = static_cast<char*>(next_occupied) - static_cast<char*>(current);
            } else {
                info.block_size = static_cast<char*>(end) - static_cast<char*>(current);
            }
        }
        
        result.push_back(info);
        current = static_cast<char*>(current) + info.block_size;
    }
    
    return result;
}

bool allocator_boundary_tags::do_is_equal(const std::pmr::memory_resource& other) const noexcept {
    return this == &other;
}

allocator_boundary_tags::boundary_iterator::boundary_iterator() 
    : _current_block(nullptr), _trusted_memory(nullptr) {}

allocator_boundary_tags::boundary_iterator::boundary_iterator(void* trusted) 
    : _trusted_memory(trusted) {
    if (trusted) {
        _current_block = static_cast<char*>(trusted) + ALLOCATOR_METADATA_SIZE;
    } else {
        _current_block = nullptr;
    }
}

bool allocator_boundary_tags::boundary_iterator::operator==(const boundary_iterator& other) const noexcept {
    return _current_block == other._current_block && _trusted_memory == other._trusted_memory;
}

bool allocator_boundary_tags::boundary_iterator::operator!=(const boundary_iterator& other) const noexcept {
    return !(*this == other);
}

allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator++() & noexcept {
    if (_current_block && _trusted_memory) {
        size_t block_size;
        if (is_occupied_block_start(_trusted_memory, _current_block)) {
            block_size = get_block_size(_current_block);
        } else {
            void* end = get_trusted_end(_trusted_memory);
            void* next_occupied = nullptr;
            void* temp = get_first_occupied(_trusted_memory);
            while (temp) {
                if (temp > _current_block) {
                    next_occupied = temp;
                    break;
                }
                temp = get_next_occupied(temp);
            }
            
            if (next_occupied) {
                block_size = static_cast<char*>(next_occupied) - static_cast<char*>(_current_block);
            } else {
                block_size = static_cast<char*>(end) - static_cast<char*>(_current_block);
            }
        }
        
        _current_block = static_cast<char*>(_current_block) + block_size;
        
        void* end = get_trusted_end(_trusted_memory);
        if (_current_block >= end) {
            _current_block = nullptr;
        }
    }
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator++(int n) {
    boundary_iterator tmp = *this;
    ++(*this);
    return tmp;
}

allocator_boundary_tags::boundary_iterator& allocator_boundary_tags::boundary_iterator::operator--() & noexcept {
    // Для простоты реализуем только инкремент
    // Декремент требует нахождения предыдущего блока, что сложнее
    return *this;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::boundary_iterator::operator--(int n) {
    boundary_iterator tmp = *this;
    --(*this);
    return tmp;
}

size_t allocator_boundary_tags::boundary_iterator::size() const noexcept {
    if (_current_block && _trusted_memory) {
        if (is_occupied_block_start(_trusted_memory, _current_block)) {
            return get_block_size(_current_block) - OCCUPIED_BLOCK_METADATA_SIZE;
        } else {
            void* end = get_trusted_end(_trusted_memory);
            void* next_occupied = nullptr;
            void* temp = get_first_occupied(_trusted_memory);
            while (temp) {
                if (temp > _current_block) {
                    next_occupied = temp;
                    break;
                }
                temp = get_next_occupied(temp);
            }
            
            if (next_occupied) {
                return static_cast<char*>(next_occupied) - static_cast<char*>(_current_block);
            } else {
                return static_cast<char*>(end) - static_cast<char*>(_current_block);
            }
        }
    }
    return 0;
}

bool allocator_boundary_tags::boundary_iterator::occupied() const noexcept {
    if (_current_block && _trusted_memory) {
        return is_occupied_block_start(_trusted_memory, _current_block);
    }
    return false;
}

void* allocator_boundary_tags::boundary_iterator::operator*() const noexcept {
    if (_current_block && occupied() && _trusted_memory) {
        return get_block_data(_current_block);
    }
    return nullptr;
}

void* allocator_boundary_tags::boundary_iterator::get_ptr() const noexcept {
    return _current_block;
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::begin() const noexcept {
    return boundary_iterator(_trusted_memory);
}

allocator_boundary_tags::boundary_iterator allocator_boundary_tags::end() const noexcept {
    return boundary_iterator(nullptr);
}