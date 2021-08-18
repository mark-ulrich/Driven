#!/bin/sh

DEBUG="-D_DEBUG -g" 

cc -D_DEBUG -g -Wall -Wno-missing-braces -o build/main src/log.c src/common.c src/memory.c src/cpu.c src/main.c
