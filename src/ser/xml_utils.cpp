#include "xml_utils.h"

#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>
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
