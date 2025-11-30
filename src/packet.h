#ifndef PACKET_H
#define PACKET_H

#include <string>

enum Flags
{
	QUIT,
	AUTH_REQUEST,
	REGISTER_REQUEST,
	ACCEPT,
	FAILURE,
	KEY_EXCHANGE
};

class Packet
{
	public:
	int size;
	int flag;
	char* data = nullptr;

	Packet();
	Packet(Flags flag, const char* data, int sz);
	~Packet();
	void Recv(int fd);
	void Send(int fd);
};

std::string Encrypt(const char* data, int sz, unsigned char key[32]);
std::string Decrypt(const char* data, int sz, unsigned char key[32]);

#endif
