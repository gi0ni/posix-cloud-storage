#ifndef PACKET_H
#define PACKET_H

#include <string>

enum Flags
{
	SUCCESS,
	FAILURE,

	QUIT,
	KEY_EXCHANGE,
	LOGIN_REQUEST,
	REGISTER_REQUEST,
	LOGOUT,

	SEND_FILE_BEGIN,
	SEND_FILE_CHUNK,
	SEND_FILE_END,
	SEND_FILE_REQUEST,

	DIR_LIST_REQUEST, // TODO: send cwd here
	CHANGE_CWD,
	CREATE_DIR,

	FILE_DELETE,
	FILE_RENAME,
	FILE_COPY,
	FILE_MOVE
};

class Packet
{
	public:
	int size;
	int flag;
	char* data = nullptr;

	Packet();
	Packet(Flags flag, const char* data, int sz);
	Packet(Flags flag, std::string msg);
	~Packet();
	void Recv(int fd);
	void Send(int fd);

	int GetDataSize();
	std::string DataToStr();
};


// encryption funcs
void RandomBytes(void* buff, int sz);

std::string Encrypt(const char* data, int sz, unsigned char key[32]);
std::string Decrypt(const char* data, int sz, unsigned char key[32]);
std::string DecryptSSL(const char* data, int sz, unsigned char key[32]);

void SendFile(const char* filepath, int socket, unsigned char key[32], bool encrypt = true);
void RecvFile(const char* filepath, int socket, unsigned char key[32], bool decrypt = true);

#endif
