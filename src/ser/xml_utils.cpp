#include "xml_utils.h"

#include <unistd.h>
#include <cstring>
#include <cerrno>

#include <stdexcept>
#include <unordered_map>

#include "utils.h"

XMLElement* FindXMLElementChild(XMLElement* parent, const std::string& filename)
{
	XMLElement* file = parent->FirstChildElement();

	while(file)
	{
		if(file->Name() == std::string("dir") && file->Attribute("name") == filename)
			return file;

		if(file->Name() == std::string("file") && file->FirstChildElement("name")->GetText() == filename)
			return file;

		file = file->NextSiblingElement();
	}

	return NULL;
}

void DeleteInferTypeXML(const std::string& userdir, XMLElement* file)
{
	if(file == NULL)
		throw std::runtime_error("File does not exit!");

	if(file->Name() == std::string("dir"))
	{
		DeleteDirXML(userdir, file);
		return;
	}

	if(file->Name() == std::string("file"))
	{
		DeleteFileXML(userdir, file);
		return;
	}
}

void DeleteFileXML(const std::string& userdir, XMLElement* file)
{
	std::string filepath = userdir + file->Attribute("id");

	if(remove(filepath.c_str()))
	{
		printf(ERR "%s\n" CLEAR, strerror(errno));
	}

	printf(OK "Delete file '%s' success.\n" CLEAR, filepath.c_str());
	XMLNode* parent = file->Parent();
	parent->DeleteChild(file);
}

void DeleteDirXML(const std::string& userdir, XMLElement* dir)
{
	XMLElement* file = dir->FirstChildElement();

	while(file)
	{
		DeleteInferTypeXML(userdir, file);
		file = dir->FirstChildElement();
	}

	printf(OK "Delete dir '%s' success.\n" CLEAR, dir->Attribute("name"));
	XMLNode* parent = dir->Parent();
	parent->DeleteChild(dir);
}

std::string FilenameMakeUniqueXML(XMLElement* parent, const std::string& filename)
{
	std::unordered_map<std::string, bool> takenNames;

	XMLElement* file = parent->FirstChildElement();

	while(file)
	{
		if(file->Name() == std::string("dir"))
			takenNames[file->Attribute("name")] = true;

		if(file->Name() == std::string("file"))
			takenNames[file->FirstChildElement("name")->GetText()] = true;

		file = file->NextSiblingElement();
	}

	if(takenNames.find(filename) == takenNames.end())
		return filename;

	int index = 1;
	while(true)
	{
		std::string candidate = filename;
		int pos = filename.find_last_of('.');
		if(pos == -1)
			pos = filename.size();

		std::string suffix = " (" + std::to_string(index) + ")";
		candidate.insert(pos, suffix);

		if(takenNames.find(candidate) == takenNames.end())
			return candidate;

		index++;
	}
}
