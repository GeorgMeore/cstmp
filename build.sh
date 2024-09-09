#!/bin/sh

cc -fsanitize=undefined,address -std=c89 -Wall -Wextra -g main.c
