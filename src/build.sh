#!/bin/bash
#gcc -g testcase2.c -o testcase2 -lhiredis -lpthread
gcc -g hiredis_connpool.c list.c test.c hiredis_api.c -o test -lhiredis -lpthread
