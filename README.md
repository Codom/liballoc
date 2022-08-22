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

Only a simple linear allocator and a dynamic arena allocator
are implemented as a proof of concept. A leak-checking
free-list allocator is on the todo list.

# Testing
The header is tested with a simple test program. The makefile
specifies this file should be built with the ubsan and addressan
enabled to ensure that these allocators don't rely on ub and
also don't produce memory leaks when used properly.

Simply ./run_tests.sh. Success output is only a printout of
the coverage report, failure should print out the line of the
test failure. Sanitizer output is also considered a failure.
