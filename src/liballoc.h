/*
 * liballoc.h
 * Copyright (C) 2022 Christopher Odom christopher.r.odom@gmail.com
 *
 * Distributed under terms of the MIT license.
 */

/*
 * This lib is trying to provide a simple interface to create
 * managed heaps in a hierarchical fashion. This provides
 * a simple way to introduce userspace heap management that
 * can not only increase memory safety but even speed up programs
 * when used properly.
 *
 * For now this is not thread safe at all.
 */

#ifndef LIBALLOC_H
#define LIBALLOC_H

#include <stddef.h>

typedef struct {
    void* (*malloc)(void*, size_t);
    void* (*realloc)(void*, void*, size_t);
    void (*free)(void*, void*);
    void* heap;
} la_alloc_callbacks_t;

// Here is the interface to la_alloc_callbacks_t structs
// that makes it a bit less strange to use.
void* la_malloc(la_alloc_callbacks_t callbacks, size_t size);
void* la_realloc(la_alloc_callbacks_t callbacks, void* ptr, size_t size);
void la_free(la_alloc_callbacks_t callbacks, void* ptr);

/*
 * We include a global definition to the libc allocator so
 * that the user can use it as a global allocator to their
 * other managed heaps.
 */
#ifdef INCLUDE_LIBC
la_alloc_callbacks_t la_libc_allocator;
#endif

/*
 * The la_linear_allocator makes a single allocation into
 * the parent allocator on init and allocates memory slices
 * linearly until it fills up. The free function is left
 * unused, the heap is expected to be freed as a whole.
 *
 * This is best used in situations where you need a known amount
 * of non-persistent temporary allocations. If the total amount of
 * memory needed is unknown see la_arena_allocator
 */
la_alloc_callbacks_t la_linear_allocator(la_alloc_callbacks_t root_allocator, size_t heap_size);
void la_linear_deinit(la_alloc_callbacks_t* res);

la_alloc_callbacks_t la_arena_allocator(la_alloc_callbacks_t parent);
void la_arena_deinit(la_alloc_callbacks_t* res);

// Implementation below
#ifdef LIBALLOC_DEF

#define unreachable() printf("Hit unreachable in %s:%d\n", __FILE__, __LINE__); exit(1)

#ifdef DEBUG
// TODO Figure out a nice way to annotate allocs and frees...
#else
void* la_malloc(la_alloc_callbacks_t callbacks, size_t size) {
    return callbacks.malloc(callbacks.heap, size);
}
void* la_realloc(la_alloc_callbacks_t callbacks, void* ptr, size_t size) {
    return callbacks.realloc(callbacks.heap, ptr, size);
}
void la_free(la_alloc_callbacks_t callbacks, void* ptr) {
    callbacks.free(callbacks.heap, ptr);
}
#endif

#ifdef INCLUDE_LIBC
#include <stdlib.h>
void* libc_malloc(void* heap, size_t size) {
    return malloc(size);

}
void* libc_realloc(void* heap, void* ptr, size_t size) {
    return realloc(ptr, size);
}
void libc_free(void* heap, void* ptr) {
    return free(ptr);
}

la_alloc_callbacks_t la_libc_allocator = {
    .malloc =  libc_malloc,
    .realloc = libc_realloc,
    .free =    libc_free,
    .heap = NULL,
};
#endif

/*
 * Notes on heap structure:
 * Heap metadata may be defined in structs such as:
 * struct {
 *     size_t size;
 *     size_t capacity;
 * };
 * These structs are expected to live inside the void* heap element of
 * the callback structure, and it is left up to callback implementations
 * to define how this is done.
 *
 * For the builtin implementations, we have it such that the heap element
 * points to the metadata structure, and offsets are computed at heap + sizeof(*_heap_t),
 * and the memory region is overallocated to include the metadata struct.
 *
 * So as for the example above:
 * heap --------> size_t size;
 *                size_t capacity;
 * heap_start --> <Begin heap contents>
 * start + size-> <End heap contents>
 *
 * More complicated allocators may choose to have multiple allocations and fully
 * manage this using the metadata state. As always, you should heavily test these
 * allocators as they could produce very nasty bugs.
 */

typedef struct {
    // Need parent free implementation
    la_alloc_callbacks_t parent_alloc;
    size_t size;
    size_t capacity;
} la_linear_heap_t;

void* la_linear_malloc(void* _heap, size_t size) {
    la_linear_heap_t *heap = (la_linear_heap_t*) _heap;

    // Fail if it cannot fit
    if(heap->size + size >= heap->capacity) {
        return NULL;
    }

    // Otherwise alloc
    void* ret = (_heap + heap->size + sizeof(la_linear_heap_t));
    heap->size += size;
    return ret;
}

// For linear allocators, the realloc function just calls it's
// malloc function. This is due to the fact that there is no
// ability to free individual allocations.
void* la_linear_realloc(void* _heap, void* _ptr, size_t size) {
    return la_linear_malloc(_heap, size);
}

void la_linear_free(void* _heap, void* _ptr) {
    return;
}

la_alloc_callbacks_t la_linear_allocator(la_alloc_callbacks_t root_allocator, size_t heap_size) {
    // Overalloc the la_linear_heap_t struct, then initialize it...
    void* heap = la_malloc(root_allocator, sizeof(la_linear_heap_t) + heap_size);
    la_linear_heap_t* lin_heap = (la_linear_heap_t*) heap;
    lin_heap->parent_alloc = root_allocator;
    lin_heap->size = 0;
    lin_heap->capacity = heap_size;

    la_alloc_callbacks_t ret = {
        .malloc = la_linear_malloc,
        .realloc = la_linear_realloc,
        .free = la_linear_free,
        .heap = heap,
    };
    return ret;
}

void la_linear_deinit(la_alloc_callbacks_t *callbacks) {
    la_linear_heap_t* lin_heap = callbacks->heap;
    la_free(lin_heap->parent_alloc, callbacks->heap);
}

/*
 * Arenas are just a linked list of linears that can be easily
 * appended to.
 */
typedef struct la_arena {
    la_linear_heap_t* bin;
    struct la_arena* next;
} la_arena_t;

typedef struct {
    // Need parent free implementation
    la_alloc_callbacks_t parent_alloc;
    la_arena_t* head;
} la_arena_heap_t;

void* la_arena_malloc(void* _heap, size_t size) {
    la_arena_heap_t  *metadata = (la_arena_heap_t*)  _heap;
    la_arena_t* prev = NULL;
    la_arena_t* p = metadata->head;
    while(p) {
        // Try the linear alloc function directly and if it failed
        // continue
        void* ret = la_linear_malloc(p->bin, size);
        if(ret) {
            // Return on success
            return ret;
        }
        prev = p;
        p = p->next;
    }

    // If we are down here, this means that all current bins cannot
    // fit the size requested. Therefore we allocate a new bin that
    // can store this allocation.
    if(!p) {
        if(prev) {
            prev->next = la_malloc(metadata->parent_alloc, sizeof(la_arena_t));
            if(!prev->next) {
                return NULL;
            }
            p = prev->next;
        } else {
            metadata->head = la_malloc(metadata->parent_alloc, sizeof(la_arena_t));
            if(!metadata->head) {
                return NULL;
            }

            p = metadata->head;
        }
        p->next = NULL;

        // P should be set to a new node, now we allocate the arena and malloc

        // We set a min bin size of 4k for small allocations, any larger
        // gets their own bin equal to 3 times the requested size.

        // TODO I should have some sort of growth heuristic in order
        // to keep track of the distribution of small and large allocations
        // and adjust accordingly
        p->bin = la_linear_allocator(metadata->parent_alloc, size > 4096? size * 3 : 4096).heap;
        if(!p->bin) {
            return NULL;
        }

        void* ret = la_linear_malloc(p->bin, size);
        return ret;
    }

    // Unreachable, force crash
    unreachable();
}

// For linear allocators, the realloc function just calls it's
// malloc function. This is due to the fact that there is no
// ability to free individual allocations.
void* la_arena_realloc(void* _heap, void* _ptr, size_t size) {
    return la_arena_malloc(_heap, size);
}

void la_arena_free(void* _heap, void* _ptr) {
    return;
}

la_alloc_callbacks_t la_arena_allocator(la_alloc_callbacks_t parent) {
    la_arena_heap_t* heap = la_malloc(parent, sizeof(la_arena_heap_t));
    if(!heap) {
        unreachable();
    }
    heap->parent_alloc = parent;
    heap->head = NULL;

    la_alloc_callbacks_t ret = {
        .malloc = la_arena_malloc,
        .realloc = la_arena_realloc,
        .free = la_arena_free,
        .heap = heap,
    };
    return ret;
}

void la_arena_deinit(la_alloc_callbacks_t* res) {
    la_arena_heap_t* heap = (la_arena_heap_t*) res->heap;
    la_alloc_callbacks_t parent = heap->parent_alloc;

    la_arena_t* p = heap->head;
    while(p) {
        la_arena_t* next = p->next;
        la_linear_heap_t* lin_heap = p->bin;
        la_free(lin_heap->parent_alloc, lin_heap);
        la_free(parent, p);
        p = next;
    }

    la_free(parent, heap);
}

#endif

#endif /* !LIBALLOC_H */
