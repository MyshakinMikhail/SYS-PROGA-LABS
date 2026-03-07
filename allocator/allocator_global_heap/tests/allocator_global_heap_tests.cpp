#include <gtest/gtest.h>
#include <iostream>
#include <allocator_global_heap.h>

TEST(allocatorGlobalHeapTests, test1)
{
    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap());
    auto block = reinterpret_cast<int *>(allocator_instance->allocate(sizeof(unsigned char) * 0));
    std::unique_ptr<smart_mem_resource> allocator_another_instance(new allocator_global_heap());
    allocator_another_instance->deallocate(block, 1);
}

TEST(allocatorGlobalHeapTests, test2)
{
    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap());
    
    auto first_block = reinterpret_cast<char *>(allocator_instance->allocate(sizeof(char) * 11));
    
    strcpy(first_block, "0123456789");
    
    allocator_instance->deallocate(first_block, 1);
}

TEST(allocatorGlobalHeapTests, test3)
{
    int const values_to_allocate_count = 30;

    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap());
    
    auto first_block = reinterpret_cast<unsigned int *>(allocator_instance->allocate(sizeof(unsigned int) * values_to_allocate_count));
    auto second_block = reinterpret_cast<unsigned int *>(allocator_instance->allocate(sizeof(unsigned long int) * values_to_allocate_count));
    auto third_block = reinterpret_cast<double *>(allocator_instance->allocate(sizeof(double) * values_to_allocate_count));
    
    for (int i = 0; i < values_to_allocate_count; i++)
    {
        first_block[i] = i;
        second_block[i] = i * i;
        third_block[i] = i / 2;
    }
    
    allocator_instance->deallocate(third_block, 1);
    allocator_instance->deallocate(first_block, 1);
    allocator_instance->deallocate(second_block, 1);
}

TEST(allocatorGlobalHeapTests, test4)
{
    int const values_to_allocate_count = 75;

    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap);
    
    auto first_block = reinterpret_cast<short *>(allocator_instance->allocate(sizeof(short) * values_to_allocate_count));
    auto second_block = reinterpret_cast<float *>(allocator_instance->allocate(sizeof(float) * values_to_allocate_count));
    
    for (int i = 0; i < values_to_allocate_count; i++)
    {
        second_block[i] = first_block[i] = (i & 1)
            ? i / 2.0
            : i;
    }
    
    allocator_instance->deallocate(first_block, 1);
    allocator_instance->deallocate(second_block, 1);
}

TEST(allocatorGlobalHeapTests, testIsEqual)
{
    std::unique_ptr<smart_mem_resource> allocator_first(new allocator_global_heap());
    std::unique_ptr<smart_mem_resource> allocator_second(new allocator_global_heap());
    
    EXPECT_TRUE(allocator_first->is_equal(*allocator_second));
    EXPECT_TRUE(allocator_second->is_equal(*allocator_first));
    EXPECT_TRUE(allocator_first->is_equal(*allocator_first));
}

TEST(allocatorGlobalHeapTests, testDeallocateNullptr)
{
    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap());
    
    EXPECT_NO_THROW(allocator_instance->deallocate(nullptr, 0));
}

TEST(allocatorGlobalHeapTests, testLargeAllocation)
{
    int const large_size = 1000000;
    
    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap());
    
    auto large_block = reinterpret_cast<long long *>(allocator_instance->allocate(sizeof(long long) * large_size));
    
    ASSERT_NE(large_block, nullptr);
    
    for (int i = 0; i < large_size; i += 10000)
    {
        large_block[i] = i * 1000LL;
    }
    
    for (int i = 0; i < large_size; i += 10000)
    {
        EXPECT_EQ(large_block[i], i * 1000LL);
    }
    
    allocator_instance->deallocate(large_block, 1);
}

TEST(allocatorGlobalHeapTests, testReallocateAfterDeallocate)
{
    int const values_count = 50;
    
    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap());
    
    auto first_block = reinterpret_cast<int *>(allocator_instance->allocate(sizeof(int) * values_count));
    for (int i = 0; i < values_count; i++)
    {
        first_block[i] = i * 2;
    }
    allocator_instance->deallocate(first_block, 1);
    
    auto second_block = reinterpret_cast<double *>(allocator_instance->allocate(sizeof(double) * values_count));
    for (int i = 0; i < values_count; i++)
    {
        second_block[i] = i * 3.14;
    }
    
    for (int i = 0; i < values_count; i++)
    {
        EXPECT_NEAR(second_block[i], i * 3.14, 0.0001);
    }
    
    allocator_instance->deallocate(second_block, 1);
}

TEST(allocatorGlobalHeapTests, testMultipleAllocationsAndDeallocations)
{
    int const iterations = 100;
    int const block_size = 20;
    
    std::unique_ptr<smart_mem_resource> allocator_instance(new allocator_global_heap());
    
    std::vector<void*> allocated_blocks;
    
    for (int iter = 0; iter < iterations; iter++)
    {
        auto block = reinterpret_cast<int *>(allocator_instance->allocate(sizeof(int) * block_size));
        ASSERT_NE(block, nullptr);
        
        for (int i = 0; i < block_size; i++)
        {
            block[i] = iter * block_size + i;
        }
        
        allocated_blocks.push_back(block);
    }
    
    for (int iter = 0; iter < iterations; iter++)
    {
        auto block = reinterpret_cast<int *>(allocated_blocks[iter]);
        for (int i = 0; i < block_size; i++)
        {
            EXPECT_EQ(block[i], iter * block_size + i);
        }
    }
    
    for (auto block : allocated_blocks)
    {
        allocator_instance->deallocate(block, 1);
    }
}

int main(
    int argc,
    char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    
    return RUN_ALL_TESTS();
}