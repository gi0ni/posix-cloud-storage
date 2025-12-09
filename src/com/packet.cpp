#include "packet.h"

#include <unistd.h>
#include <fcntl.h>

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <cassert>

#include "utils.h"

#include "sodium.h"
#include <openssl/evp.h>

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

Packet::Packet(Flags flag, std::string msg)
{
	this->flag = flag;

	this->data = (char*)malloc(msg.size());
	memcpy(this->data, msg.data(), msg.size());

	this->size = 8 + msg.size();
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
	if(write(fd, this, 8) <= 0)
		throw std::runtime_error(std::string(strerror(errno)));
	
	if(size > 8)
		if(write(fd, data, size - 8) <= 0)
			throw std::runtime_error(std::string(strerror(errno)));
}

int Packet::GetDataSize()
{
	return size - 8;
}

std::string Packet::DataToStr()
{
	if(size == 0)
		return "";
	return std::string(data, size - 8);
}


void RandomBytes(void* buff, int sz)
{
	int fd = open("/dev/random", O_RDONLY);
	read(fd, buff, sz);
	close(fd);
}

std::string Encrypt(const char* data, int sz, unsigned char key[32])
{
	std::string output(sz + 16, 0);
	unsigned long long int clen;
	unsigned char nonce[12];

	RandomBytes(nonce, 12);

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

	// std::cout << WARN "Sending encrypted message:\n" CLEAR;
	// std::cout << "     nonce: "; HexDump(std::string(&final_output[0], 12));
	// std::cout << "cyphertext: "; HexDump(std::string(&final_output[12], final_output.size() - 12 - 16));
	// std::cout << "       MAC: "; HexDump(std::string(&final_output[final_output.size() - 16], 16));
	// std::cout << '\n';

	// return (char*)nonce + output; OMFG
	return final_output;
}

std::string DecryptSSL(const char* data, int sz, unsigned char key[32])
{
	std::string plaintext(sz - 12 - 16, 0);
	int outlen;

	unsigned char nonce[12]; memcpy((char*)nonce, data, 12);
	std::string cyphertext = std::string(data, sz).substr(12, sz - 12 - 16); // FORGOT TO ADD SZ MIGHT HAVE FIXED THE OTHER ISSUE TOO
	std::string tag = std::string(data + sz - 16, 16);

	// int err = crypto_aead_aes256gcm_decrypt(
	// 		(unsigned char*)&output[0],
	// 		&mlen,
	// 		NULL,
	// 		(const unsigned char*)(data + 12),
	// 		sz - 12,
	// 		NULL,
	// 		0,
	// 		nonce,
	// 		key
	// );

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce)) goto error;
	printf("A\n");

	if(!EVP_DecryptUpdate(ctx, (unsigned char*)&plaintext[0], &outlen, (unsigned char*)&cyphertext[0], cyphertext.size())) goto error;
	printf("C\n");

	if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, &tag[0])) goto error;
	printf("D\n");

	if(EVP_DecryptFinal_ex(ctx, (unsigned char*)(plaintext.data() + outlen), &outlen) <= 0) goto error;
	printf("E\n");

	printf("F\n");
	EVP_CIPHER_CTX_free(ctx);

	// std::cout << "AES MESSAGE:\n";
	// std::cout << "nonce     : "; HexDump(std::string((char*)data, 12));
	// std::cout << "cyphertext: "; HexDump(std::string((char*)(data + 12), sz - 12 - 16));
	// std::cout << "MAC       : "; HexDump(std::string((char*)(data + sz - 16), 16));

	std::cout << "OMG IT WORKED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
	return plaintext;

	error:
	EVP_CIPHER_CTX_free(ctx);
	printf("DECRYPT ERROR OH NO\n");
	return "decryptErrorThisIsBad";
}

// FIX: does not work over ssh. use OpenSSL instead.
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

	// std::cout << "AES MESSAGE:\n";
	// std::cout << "nonce     : "; HexDump(std::string((char*)data, 12));
	// std::cout << "cyphertext: "; HexDump(std::string((char*)(data + 12), sz - 12 - 16));
	// std::cout << "MAC       : "; HexDump(std::string((char*)(data + sz - 16), 16));

	if(err)
		printf("DECRYPT ERROR OH NO\n");

	return output;
}

#define CHUNK_SIZE 512
#define DISABLE_CRYPTO 0

void SendFile(const char* filepath, int socket, unsigned char key[32], bool encrypt)
// think of encrypt as - is the file already encrypted?
{
	std::cout << "SENDING TO CLIENT: " << filepath << '\n';
	int fd = open(filepath, O_RDONLY);

	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	std::string base(basename(filepath));

	// FIX: use a hash for this so you don't go over NAMEMAX 255B
	// std::string filenameenc = Encrypt(base.data(), base.size(), glb.fileKey);
	// filenameenc = ToHexString(filenameenc.data(), filenameenc.size());
	//
	// std::string filenamehash;
	// crypto_hash_sha256((unsigned char*)filenamehash.data(), (unsigned char*)base.c_str(), base.size());
	// filenamehash = ToHexString(filenamehash.data(), filenamehash.size());

	std::string data = std::string(basename(filepath)) + "\n" + std::to_string(size) + "\n";
	Packet packet(Flags::SEND_FILE_BEGIN, &data[0], data.size());
	packet.Send(socket);

	packet.data = (char*)realloc(packet.data, CHUNK_SIZE + 12 + 16);

	Packet response;

	while(size > 0)
	{
		// not ideal

		// packet.flag = Flags::FILE_CHUNK;
		// int bytes = read(fd, packet.data, 4096);

		packet.flag = Flags::SEND_FILE_CHUNK;

		int bytes;

		if(encrypt && !DISABLE_CRYPTO)
		{
			bytes = read(fd, packet.data, CHUNK_SIZE);

			std::string encryptedData = Encrypt(packet.data, bytes, key);
			memcpy(packet.data, &encryptedData[0], encryptedData.size());
			assert(encryptedData.size() == bytes + 12 + 16);
			packet.size = encryptedData.size() + 8;
		}
		else
		{
			// this assumes the file was already encrypred. the function is not that clear with enc = false. not ideal
			packet.flag = Flags::SEND_FILE_CHUNK;
			bytes = read(fd, packet.data, CHUNK_SIZE + 12 + 16);
			packet.size = bytes + 8;
		}

		size -= bytes;

		if(size < 0)
			std::cout << "SIZE IS NEGATIVE. SOMETHING IS WRONG.\n";

		packet.Send(socket);
		response.Recv(socket);
		std::cout << "sending file chunk! size is " << packet.size << "left is: " << size << '\n';
	}

	packet.flag = Flags::SEND_FILE_END;
	free(packet.data);
	packet.data = nullptr;
	packet.size = 8;
	packet.Send(socket);

	close(fd);
}

// should make the server use this too
// I COULD ALSO JUST make 4 functions GetFileDecrypt GetFile SendFileEncrypt SendFile. that's stupid maybe not
void RecvFile(const char* filepath, int socket, unsigned char key[32], bool decrypt)
{

	// could send this from outside the func
	Packet packet(Flags::SEND_FILE_REQUEST, filepath, strlen(filepath));
	packet.Send(socket);

	packet.Recv(socket); // FILE_SEND_BEGIN ignore for now
	assert(packet.flag == Flags::SEND_FILE_BEGIN);

	std::string downpath = getenv("HOME");
	downpath += "/Downloads/";
	downpath += filepath;

	// FIX: check if filename is taken. if it is rename the file to file(1).txt or whatever

	int fd = open(downpath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);

	while(1)
	{
		packet.Recv(socket);

		if(packet.flag == Flags::SEND_FILE_END)
			break;

		std::string chunk;

		if(decrypt && !DISABLE_CRYPTO)
		{
			chunk = Decrypt(packet.data, packet.size - 8, key);
		}
		else
		{
			chunk = std::string(packet.data, packet.size - 8);
		}

		write(fd, chunk.data(), chunk.size());
		std::cout << "receiving file chunk! size is " << packet.size << '\n';
		std::cout << chunk.data() << '\n';
		Packet response(Flags::SUCCESS, NULL, 0);
		response.Send(socket);
	}

	close(fd);
}
