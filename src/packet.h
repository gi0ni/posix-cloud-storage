#ifndef PACKET_H
#define PACKET_H

enum Flags
{
	QUIT,
	TEST = 127
};

class Packet
{
	public:
	int size;
	int flag;
	char* data;

	Packet();
	Packet(Flags flag, const char* data, int sz);
	~Packet();
	void Recv(int fd);
	void Send(int fd);
};


#endif
