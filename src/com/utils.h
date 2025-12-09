#ifndef UTILS_H
#define UTILS_H

#include <string>

#define ERR   "\e[31m[ERROR] "
#define WARN  "\e[33m"
#define CLEAR "\e[0m"
#define OK    "\e[32m[OK] "

void PrintErr(const char* msg);

void HexDump(const char* data, int sz);
void HexDump(const std::string& data);

std::string FromHexString(const char* data, int sz);
std::string ToHexString(const char* data, int sz);

bool IsDirectory(const char* name);

#endif
