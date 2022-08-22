#
# Makefile
# Christopher Odom, 2022-08-20 07:44
#

SAFETY=-fsanitize=address,undefined

# Tests are inherently testable binaries with all of the compiler
# safety checks enabled
test: src/test_main.c src/liballoc.h
	$(CC) $(SAFETY) -g --coverage src/test_main.c -o ./test

clean:
	rm ./test

# vim:ft=make
#
