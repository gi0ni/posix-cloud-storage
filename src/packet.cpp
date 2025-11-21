#include "packet.h"

Packet RecvPacket(int fd)
{
	Packet temp;
	if(read(fd, &temp.size, sizeof(temp.size)) < 0)
		throw std::runtime_error("Read failure");

	if(read(fd, (char*)(&temp) + sizeof(temp.size), temp.size - sizeof(temp.size)) < 0)
		throw std::runtime_error("Read failure");

	return temp;
}

void SendPacket(const Packet& packet, int fd)
{
	if(write(fd, &packet, packet.size) < 0)
		throw std::runtime_error("Write failure");
}
