#! /bin/sh
#
# run_tests.sh
# Copyright (C) 2022 Christopher Odom christopher.r.odom@gmail.com
#
# Distributed under terms of the MIT license.
#

make test && ./test && gcov ./test-test_main.gcda
