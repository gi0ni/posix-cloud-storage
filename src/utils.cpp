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
