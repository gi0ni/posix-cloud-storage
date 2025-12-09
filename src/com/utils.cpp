#include "utils.h"

void PrintErr(const char* msg)
{
	fprintf(stderr, ERR);
	perror(msg);
	fprintf(stderr, CLEAR);
}

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
// #include <algorithm>
#include <dirent.h>
// #include <filesystem>
#include <sstream>

#include <iostream>
#include <iomanip>

// namespace fs = std::filesystem;
//
// std::vector<std::string> Crawl(std::string dirname)
// {
// 	std::vector<std::string> dirContents;
//
// 	DIR* dir = opendir(dirname.c_str());
//
// 	dirent* entry;
// 	while((entry = readdir(dir)))
// 	{
// 		dirContents.push_back(std::string(entry->d_name));
// 	}
//
// 	closedir(dir);
//
// 	std::sort(dirContents.begin(), dirContents.end(), [dirname](const std::string& left, const std::string& right) {
//
// 		std::string l = dirname + "/" + left;
// 		std::string r = dirname + "/" + right;
//
// 		if(fs::is_directory(l) && !fs::is_directory(r))
// 			return true;
// 		if(!fs::is_directory(l) && fs::is_directory(r))
// 			return false;
//
// 		return left < right;
// 	});
// 	return dirContents;
// }

void HexDump(const char* data, int sz)
{
	std::ios init(NULL);
	init.copyfmt(std::cout);

	for(int i = 0; i < sz; i++)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)data[i];
	std::cout << '\n';

	std::cout.copyfmt(init);
}

void HexDump(const std::string& data)
{
	std::ios init(NULL);
	init.copyfmt(std::cout);

	for(int i = 0; i < data.size(); i++)
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)data[i];
	std::cout << '\n';

	std::cout.copyfmt(init);
}

//////////////////// for xml
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
/////////////////////

bool isdir(const char* name)
{
	struct stat st;
	if(stat(name, &st))
		return false;

	if(S_ISDIR(st.st_mode))
		return true;
	return false;
}

