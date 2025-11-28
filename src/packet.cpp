#include "packet.h"

#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <iostream>

Packet::Packet()
{
	data = nullptr;
}

Packet::Packet(Flags flag, const char* data, int sz)
{
	this->flag = flag;

	if(sz != 0)
	{
		this->data = (char*)realloc(this->data, sz);
		strncpy(this->data, data, sz);
	}
	else
	{
		this->data = nullptr;
	}

	this->size = 8 + sz;
}

Packet::~Packet()
{
	if(data != nullptr)
		free(data);
}

void Packet::Recv(int fd)
{
	if(read(fd, &size, sizeof(size)) < 0)
		throw std::runtime_error("Read failure");

	if(size > 8) data = (char*)realloc(data, size);

	if(read(fd, (char*)(this) + sizeof(size), size - sizeof(size)) < 0)
		throw std::runtime_error("Read failure");
}

void Packet::Send(int fd)
{
	if(write(fd, this, size) < 0)
		throw std::runtime_error("Write failure");
}
