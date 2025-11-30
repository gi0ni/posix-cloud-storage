#include "utils.h"

void PrintErr(const char* msg)
{
	fprintf(stderr, ERR);
	perror(msg);
	fprintf(stderr, CLEAR);
}

#include <algorithm>
#include <dirent.h>
#include <filesystem>

#include <iostream>
#include <iomanip>

namespace fs = std::filesystem;

std::vector<std::string> Crawl(std::string dirname)
{
	std::vector<std::string> dirContents;

	DIR* dir = opendir(dirname.c_str());

	dirent* entry;
	while((entry = readdir(dir)))
	{
		dirContents.push_back(std::string(entry->d_name));
	}

	closedir(dir);

	std::sort(dirContents.begin(), dirContents.end(), [dirname](const std::string& left, const std::string& right) {

		std::string l = dirname + "/" + left;
		std::string r = dirname + "/" + right;

		if(fs::is_directory(l) && !fs::is_directory(r))
			return true;
		if(!fs::is_directory(l) && fs::is_directory(r))
			return false;

		return left < right;
	});
	return dirContents;
}

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
