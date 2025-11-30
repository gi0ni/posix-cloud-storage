#include "packet.h"

#include <unistd.h>
#include <fcntl.h>

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <iostream>

#include "utils.h"

#include <sodium/crypto_aead_aes256gcm.h>

Packet::Packet()
{
	// data = nullptr;
}

Packet::Packet(Flags flag, const char* data, int sz)
{
	this->flag = flag;

	if(sz != 0)
	{
		this->data = (char*)malloc(sz);
		// strncpy(this->data, data, sz); // STRNCPY ALSO STOPS AT NULL. THEY DONT TELL ME THESE THINGS
		memcpy(this->data, data, sz);
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
	{
		free(data);
		data = nullptr;
	}
}

void Packet::Recv(int fd)
{
	if(read(fd, &size, 4) <= 0)
		throw std::runtime_error(std::string(strerror(errno)));

	std::cout << "THE SIZE IS: " << size << '\n';

	if(read(fd, &flag, 4) <= 0)
		throw std::runtime_error(std::string(strerror(errno)));

	if(size > 8)
	{
		data = (char*)realloc(data, size - 8);
		if(read(fd, data, size - 8) <= 0)
			throw std::runtime_error(std::string(strerror(errno)));
	}
}

void Packet::Send(int fd)
{
	if(write(fd, this, 8) < 0)
		throw std::runtime_error(std::string(strerror(errno)));
	
	if(size > 8)
		if(write(fd, data, size - 8) < 0)
			throw std::runtime_error(std::string(strerror(errno)));
}

std::string Encrypt(const char* data, int sz, unsigned char key[32])
{
	std::string output(sz + 16, 0);
	unsigned long long int clen;
	unsigned char nonce[12];

	int fd = open("/dev/random", O_RDONLY);
	read(fd, nonce, 12);
	close(fd);

	crypto_aead_aes256gcm_encrypt(
			(unsigned char*)&output[0],
			&clen,
			(unsigned char*)data,
			sz,
			NULL,
			0,
			NULL,
			nonce,
			key
			);

	std::string final_output = std::string((char*)nonce, 12) + output;

	std::cout << "AES MESSAGE:\n";
	std::cout << "nonce     : "; HexDump(std::string(&final_output[0], 12));
	std::cout << "cyphertext: "; HexDump(std::string(&final_output[12], final_output.size() - 12 - 16));
	std::cout << "MAC       : "; HexDump(std::string(&final_output[final_output.size() - 16], 16));

	// return (char*)nonce + output; OMFG
	return final_output;
}

// FIX: looks like this randomly doesnt work either
std::string Decrypt(const char* data, int sz, unsigned char key[32])
{
	std::string output(sz - 12 - 16, 0);
	unsigned long long int mlen;
	unsigned char nonce[12]; memcpy((char*)nonce, data, 12);

	int err = crypto_aead_aes256gcm_decrypt(
			(unsigned char*)&output[0],
			&mlen,
			NULL,
			(const unsigned char*)(data + 12),
			sz - 12,
			NULL,
			0,
			nonce,
			key
	);

	std::cout << "AES MESSAGE:\n";
	std::cout << "nonce     : "; HexDump(std::string((char*)data, 12));
	std::cout << "cyphertext: "; HexDump(std::string((char*)(data + 12), sz - 12 - 16));
	std::cout << "MAC       : "; HexDump(std::string((char*)(data + sz - 16), 16));

	if(err)
		printf("DECRYPT ERROR OH NO\n");

	return output;
}
