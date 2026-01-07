/*
 * test_main.c
 * Copyright (C) 2022 MYNAME <EMAIL>
 *
 * Distributed under terms of the MIT license.
 */
#include <stdio.h>
#include <string.h>

#define LIBALLOC_DEF
#define INCLUDE_LIBC
#include "liballoc.h"

#define assert(a) if(!(a)) { printf("Assertion failure in %s:%d\n", __FILE__, __LINE__); exit(1); }

// ASan-safe tests: verify allocator correctness without OOB access
void linear_allocator_test() {
    // Basic allocation test
    la_alloc_callbacks_t linear_alloc = la_linear_allocator(la_libc_allocator, 32 * 1024);
    float* tmp_list = la_malloc(linear_alloc, sizeof(float) * 1024);
    float* another_list = la_malloc(linear_alloc, sizeof(float) * 10);
    assert(tmp_list != NULL);
    assert(another_list != NULL);

    // Verify we can write within bounds
    tmp_list[0] = 42.0f;
    tmp_list[1023] = 100.0f;
    another_list[0] = 1.0f;
    another_list[9] = 2.0f;
    assert(tmp_list[0] == 42.0f);
    assert(tmp_list[1023] == 100.0f);

    la_linear_deinit(&linear_alloc);

    // Capacity exhaustion test
    linear_alloc = la_linear_allocator(la_libc_allocator, 1024);
    void* should_fail = la_malloc(linear_alloc, 2048);
    assert(should_fail == NULL);

    // With 8-byte alignment, we have less usable space
    int* should_pass = la_malloc(linear_alloc, sizeof(int) * 200);
    assert(should_pass != NULL);

    // Fill the rest of capacity
    should_fail = la_malloc(linear_alloc, 1024);
    assert(should_fail == NULL);
    la_linear_deinit(&linear_alloc);

    // Realloc test
    linear_alloc = la_linear_allocator(la_libc_allocator, 1024);
    should_pass = la_malloc(linear_alloc, sizeof(int) * 100);
    assert(should_pass);

    // This will fail because there is not enough space
    should_fail = la_realloc(linear_alloc, should_pass, 2048);
    assert(!should_fail);

    la_free(linear_alloc, should_fail);
    la_linear_deinit(&linear_alloc);
}

void basic_libc_usage_test() {
    // Libc allocator should always work
    int* myptr = la_malloc(la_libc_allocator, sizeof(int) * 10);
    assert(myptr);
    la_free(la_libc_allocator, myptr);
}

// ASan-safe tests: verify arena allocator correctness without OOB access
void arena_allocator_test() {
    // Basic usage test
    la_alloc_callbacks_t arena_allocator = la_arena_allocator(la_libc_allocator);
    int* should_pass = la_malloc(arena_allocator, sizeof(int) * 10);
    assert(should_pass);

    // Write within bounds
    for (int i = 0; i < 10; i++) {
        should_pass[i] = i;
    }
    assert(should_pass[0] == 0);
    assert(should_pass[9] == 9);

    should_pass = la_realloc(arena_allocator, should_pass, sizeof(int) * 100);
    assert(should_pass);
    la_free(arena_allocator, should_pass);
    la_arena_deinit(&arena_allocator);

    // Multiple allocations test
    arena_allocator = la_arena_allocator(la_libc_allocator);
    int* first_array = la_malloc(arena_allocator, sizeof(int) * 10);
    int* middle_array = la_malloc(arena_allocator, sizeof(int) * 10);
    int* end_array = la_malloc(arena_allocator, sizeof(int) * 10);

    assert(first_array != NULL);
    assert(middle_array != NULL);
    assert(end_array != NULL);

    // Verify we can write within bounds
    first_array[0] = 1;
    first_array[9] = 10;
    middle_array[0] = 11;
    middle_array[9] = 20;
    end_array[0] = 21;
    end_array[9] = 30;

    assert(first_array[0] == 1);
    assert(middle_array[9] == 20);
    assert(end_array[9] == 30);

    // Large allocation test - should get its own bin
    int* large_array = la_malloc(arena_allocator, sizeof(int) * 4096);
    assert(large_array != NULL);
    large_array[0] = 0xdeadbeef;
    large_array[4095] = 0xcafebabe;
    assert(large_array[0] == 0xdeadbeef);
    assert(large_array[4095] == 0xcafebabe);

    la_arena_deinit(&arena_allocator);
}

int main(int argc, char* argv[]) {
    basic_libc_usage_test();
    linear_allocator_test();
    arena_allocator_test();
}
