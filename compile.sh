#!/bin/bash

mpicc -Wall -fPIC -lpapi -c mini.c
gcc -shared -o libmini.so mini.o
