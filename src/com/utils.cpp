#include "utils.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include <iostream>
#include <iomanip>
#include <sstream>


void PrintErr(const char* msg)
{
	fprintf(stderr, ERR);
	perror(msg);
	fprintf(stderr, CLEAR);
}


void HexDump(const char* data, int sz)
{
	std::ios init(NULL);
	init.copyfmt(std::cout);

	std::cout << std::hex << std::setw(2) << std::setfill('0');
	for(int i = 0; i < sz; i++)
		std::cout << (int)(unsigned char)(data[i]);
	std::cout << '\n';

	std::cout.copyfmt(init);
}


std::string ToHexString(const char* data, int sz)
{
	std::stringstream stream;

	for(int i = 0; i < sz; i++)
		stream << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)data[i];

	return stream.str();
}

std::string FromHexString(const char* data, int sz)
{
	std::string output;

	for(int i = 0; i < sz; i += 2)
	{
		int dig0 = (data[i + 0] >= '0' && data[i + 0] <= '9' ? data[i + 0] - '0' : data[i + 0] - 'a' + 10);
		int dig1 = (data[i + 1] >= '0' && data[i + 1] <= '9' ? data[i + 1] - '0' : data[i + 1] - 'a' + 10);

		int val = 16 * dig0 + dig1;
		output.push_back(val);
	}

	return output;
}


bool IsDirectory(const char* name)
{
	struct stat st;
	if(stat(name, &st))
		return false;

	if(S_ISDIR(st.st_mode))
		return true;
	return false;
}
