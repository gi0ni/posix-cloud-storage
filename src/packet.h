#ifndef PACKET_H
#define PACKET_H

#include <unistd.h>
#include <stdexcept>

enum Flags
{
	QUIT
};

struct Packet
{
	int size;
	int flag;
	char* data;
};

Packet RecvPacket(int fd);
void SendPacket(const Packet& packet, int fd);

#endif
