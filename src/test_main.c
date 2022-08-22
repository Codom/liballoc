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

void linear_allocator_test() {
    // Linear allocators act as explicit linear buffers
    // with a known lifetime and size. As such, these are
    // useful for providing limited temporary storage for
    // algorithms with known constraints.
    //
    // NOTE Since this is a linear allocator it can hypothetically
    // produce some interesting (infuriating) bugs if you fail to do proper
    // bounds checking. So do bounds checking always.
    la_alloc_callbacks_t linear_alloc = la_linear_allocator(la_libc_allocator, 32 * 1024);
    float* tmp_list = la_malloc(linear_alloc, sizeof(int) * 1024);
    float* another_list = la_malloc(linear_alloc, sizeof(float) * 10);
    assert(tmp_list != NULL);
    assert(another_list != NULL);

    // Let's live dangerously :^)
    tmp_list[1024] = 42;
    assert(another_list[0] == 42);

    // End of previous "lifetime" no need to free the above
    // objects since the heap is freed all at once
    la_linear_deinit(&linear_alloc);

    linear_alloc = la_linear_allocator(la_libc_allocator, 1024);
    void* should_fail = la_malloc(linear_alloc, 2048);
    assert(should_fail == NULL);
    int* should_pass = la_malloc(linear_alloc, sizeof(int) * (1024/4) - 1);
    assert(should_pass != NULL);
    should_fail = la_malloc(linear_alloc, sizeof(int));
    assert(should_fail == NULL);
    la_linear_deinit(&linear_alloc);

    linear_alloc = la_linear_allocator(la_libc_allocator, 1024);

    should_pass = la_malloc(linear_alloc, sizeof(int) * (1024/4 - 1));
    assert(should_pass);

    // This will fail because there is not enough space to allocate 10 ints
    should_fail = la_realloc(linear_alloc, should_pass, sizeof(int)*10);
    assert(!should_fail);

    // Just 4 dat completion baby
    la_free(linear_alloc, should_fail);
    
    la_linear_deinit(&linear_alloc);
}

void basic_libc_usage_test() {
    // Libc allocator should always work
    int* myptr = la_malloc(la_libc_allocator, sizeof(int) * 10);
    assert(myptr);
    la_free(la_libc_allocator, myptr);
}

void arena_allocator_test() {
    // Basic usage test
    la_alloc_callbacks_t arena_allocator = la_arena_allocator(la_libc_allocator);
    int* should_pass = la_malloc(arena_allocator, sizeof(int) * 10);
    assert(should_pass);
    should_pass = la_realloc(arena_allocator, should_pass, sizeof(int) * 100);
    assert(should_pass);
    la_free(arena_allocator, should_pass);
    la_arena_deinit(&arena_allocator);

    // Bounds test
    // These allocations are less than 4k and should fit in the same bin, as
    // such they should all be adjacent
    arena_allocator = la_arena_allocator(la_libc_allocator);
    int* first_array = la_malloc(arena_allocator, sizeof(int) * 10);
    int* middle_array = la_malloc(arena_allocator, sizeof(int) * 10);
    int* end_array = la_malloc(arena_allocator, sizeof(int) * 10);

    // Ensure no random passing because of uninit values
    memset(first_array, 0xffffffff, 10);
    memset(middle_array, 0xffffffff, 10);
    memset(end_array, 0xffffffff, 10);

    first_array[10] = 10;
    assert(middle_array[0] == 10);
    middle_array[10] = 10;
    assert(end_array[0] == 10);

    // This is a large allocation and should exist in it's own
    // arena, and the out of bounds access shouldn't effect
    // the large array
    int* large_array = la_malloc(arena_allocator, sizeof(int) * 4096);
    memset(large_array, 0xffffffff, 4096);
    end_array[10] = 10;
    assert(large_array[0] == 0xffffffff);

    la_arena_deinit(&arena_allocator);
}

int main(int argc, char* argv[]) {
    basic_libc_usage_test();
    linear_allocator_test();
    arena_allocator_test();
}
