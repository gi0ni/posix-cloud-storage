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
	KEY_EXCHANGE,
	SEND_FILE_BEGIN,
	FILE_CHUNK,
	SEND_FILE_END,

	DIR_LIST_REQUEST,
	FILE_REQUEST,
	DIR_LIST,
	CHANGE_DIR,
	FILE_REMOVE,
	REQUEST_BACKUP
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
	// GetDataSize
};

std::string Encrypt(const char* data, int sz, unsigned char key[32]);
std::string Decrypt(const char* data, int sz, unsigned char key[32]);
void SendFile(const char* filepath, int socket, unsigned char key[32], bool encrypt = true);
void RecvFile(const char* filepath, int socket, unsigned char key[32], bool decrypt = true);

#endif
