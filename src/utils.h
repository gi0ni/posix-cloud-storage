#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include <string>
#include <vector>

#define ERR "\e[31m[ERROR] "
#define WARN "\e[33m"
#define CLEAR "\e[0m"
#define OK "\e[32m[OK] "

void PrintErr(const char* msg);
std::vector<std::string> Crawl(std::string dirname);
void HexDump(const char* data, int sz);
void HexDump(const std::string& data);
bool isdir(const char* name);
std::string FromHexString(const char* data, int sz);
std::string ToHexString(const char* data, int sz);

#endif
