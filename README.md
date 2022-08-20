This header is a first pass at implementing something akin
to zig's std.mem.Allocator api but in C. This api allows for
the trivial definition of userspace managed heaps that can
allow for easier memory debugging, lifetime management, and
speed improvements over the libC allocator.

On top of exposing definitions for the internal allocators,
a callback api is also provided to allow for allocators to use
parent allocators in order to make things like dynamically
resizing arenas possible. While this may not be ideal for
applications needing predictable icache behavior, it is
ideal for applications needing something better than the
libc allocator.

Only a simple linear allocator is implemented, mostly
just as a proof of concept. A leak-checking
free-list allocator and dynamically resizing arena allocators
could be implemented in this, and may be in the future.
