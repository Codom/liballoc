/*
 * test_main.c
 * Copyright (C) 2022 MYNAME <EMAIL>
 *
 * Distributed under terms of the MIT license.
 */
#include <stdio.h>

#define LIBALLOC_DEF
#define INCLUDE_LIBC
#include "liballoc.h"


int main(int argc, char* argv[]) {
    int* myptr = la_malloc(la_libc_allocator, sizeof(int) * 10);
    for(int i = 0; i < 10; ++i) {
        myptr[i] = i * i;
    }
    for(int i = 0; i < 10; ++i) {
        printf("myptr[%d] = %d\n", i, myptr[i]);
    }
    la_free(la_libc_allocator, myptr);


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
    for(int i = 0; i < 10; ++i) {
        another_list[i] = 1.0 / ((float) (i+1));
    }
    for(int i = 0; i < 1024; ++i) {
        tmp_list[i] = 1.0 / ((float) (i+1));
    }
    for(int i = 0; i < 10; ++i) {
        printf("another_list[%d] = %f\n", i, another_list[i]);
    }

    // Let's live dangerously :^)
    printf("Just to demonstrate bounds checking failure, I modified tmp_list[1024] which should be == another_list[0]\n");
    tmp_list[1024] = 42;
    printf("another_list[0] = %f\n", another_list[0]);
    printf("Always bounds check :^)\n");

    // End of previous "lifetime" no need to free the above
    // objects since the heap is freed all at once
    la_linear_allocator_free(&linear_alloc);
}
