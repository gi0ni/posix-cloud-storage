#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#define ERR "\e[31m[ERROR] "
#define WARN "\e[33m"
#define CLEAR "\e[0m"
#define OK "\e[32m[OK] "

void PrintErr(const char* msg);

#endif
