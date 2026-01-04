#pragma once

#include <tinyxml2.h>
using namespace tinyxml2;

#include <string>

XMLElement* FindXMLElementChild(XMLElement* parent, const std::string& filename);
void DeleteFileXML(const std::string& userdir, XMLElement* file);
void DeleteDirXML(const std::string& userdir, XMLElement* dir);
void DeleteInferTypeXML(const std::string& userdir, XMLElement* file);
