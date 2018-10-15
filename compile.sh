#!/bin/bash

mpicc -Wall -fPIC -lpapi -DBUFSIZE=120000 -DBUFCNT=250 -DTMPSIZE=10000 -c mini.c
gcc -shared -o libmini.so mini.o
