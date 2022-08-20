#
# Makefile
# MYNAME, 2022-08-20 07:44
#

test: src/test_main.c
	$(CC) -g src/test_main.c -o ./test

# vim:ft=make
#
