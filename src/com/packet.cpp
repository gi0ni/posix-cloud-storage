#include "packet.h"

#include <unistd.h>
#include <fcntl.h>

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <iostream>
#include <stdexcept>

#include "utils.h"

#include <sodium.h>
#include <openssl/evp.h>


std::string FlagToStr(Flags flag)
{
	switch(flag)
	{
		case SUCCESS:           return "SUCCESS";
		case FAILURE:           return "FAILURE";

		case QUIT:              return "QUIT";
		case KEY_EXCHANGE:      return "KEY_EXCHANGE";
		case LOGIN_REQUEST:     return "LOGIN_REQUEST";
		case REGISTER_REQUEST:  return "REGISTER_REQUEST";
		case LOGOUT:            return "LOGOUT";

		case SEND_FILE_BEGIN:   return "SEND_FILE_BEGIN";
		case SEND_FILE_CHUNK:   return "SEND_FILE_CHUNK";
		case SEND_FILE_END:     return "SEND_FILE_END";
		case FILE_REQUEST:      return "FILE_REQUEST";

		case DIR_LIST_REQUEST:  return "DIR_LIST_REQUEST";
		case CHANGE_CWD:        return "CHANGE_CWD";
		case CREATE_DIR:        return "CREATE_DIR";

		case FILE_DELETE:       return "FILE_DELETE";
		case FILE_RENAME:       return "FILE_RENAME";
		case FILE_COPY:         return "FILE_COPY";
		case FILE_MOVE:         return "FILE_MOVE";
	    default: return "UNKNOWN_FLAG";
	}
}


Packet::Packet()
{
	data = nullptr;
}

Packet::Packet(Flags flag, const char* data, int sz)
{
	this->flag = flag;

	if(sz != 0)
	{
		this->data = (char*)malloc(sz);
		memcpy(this->data, data, sz);
	}
	else
	{
		this->data = nullptr;
	}

	this->size = 8 + sz;
}

Packet::Packet(Flags flag, const std::string& msg)
{
	this->flag = flag;

	if(msg.size() != 0)
	{
		this->data = (char*)malloc(msg.size());
		memcpy(this->data, msg.data(), msg.size());
	}
	else
	{
		this->data = nullptr;
	}

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


int ReadExactBytes(int fd, void* ptr, int bytes)
{
	int totalBytes = 0;
	int iterationCount = 0;

	while(totalBytes < bytes)
	{
		int bytesRead = read(fd, (char*)(ptr) + totalBytes, bytes - totalBytes);
		if(bytesRead < 0) // <= ?
			throw std::runtime_error(std::string(strerror(errno)));

		totalBytes += bytesRead;

		iterationCount++;
		if(iterationCount >= 100000)
			throw std::runtime_error(std::string("Read passed iteration limit of 100000!"));
	}

	return totalBytes;
}

int WriteExactBytes(int fd, void* ptr, int bytes)
{
	int totalBytes = 0;
	int iterationCount = 0;

	while(totalBytes < bytes)
	{
		int bytesRead = write(fd, (char*)(ptr) + totalBytes, bytes - totalBytes);
		if(bytesRead < 0) // <= ?
			throw std::runtime_error(std::string(strerror(errno)));

		totalBytes += bytesRead;

		iterationCount++;
		if(iterationCount >= 100000)
			throw std::runtime_error(std::string("Write passed iteration limit of 100000!"));
	}

	return totalBytes;
}


void Packet::Recv(int fd)
{
	ReadExactBytes(fd, &size, 4);
	ReadExactBytes(fd, &flag, 4);

	if(size > 8)
	{
		data = (char*)realloc(data, size - 8);
		ReadExactBytes(fd, data, size - 8);
	}
}

void Packet::Send(int fd)
{
	WriteExactBytes(fd, &size, 4);
	WriteExactBytes(fd, &flag, 4);
	
	if(size > 8)
	{
		WriteExactBytes(fd, data, size - 8);
	}
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


/////////////// ENCRYPTION FUNCS ///////////////
void RandomBytes(void* buff, int sz)
{
	int fd = open("/dev/random", O_RDONLY);
	read(fd, buff, sz);
	close(fd);
}

std::string Encrypt(const char* data, int sz, unsigned char key[32])
{
	std::string output(sz + 16, 0);
	unsigned long long int outputLen;
	unsigned char nonce[12];

	RandomBytes(nonce, 12);

	crypto_aead_aes256gcm_encrypt(
			(unsigned char*)&output[0],
			&outputLen,
			(unsigned char*)data,
			sz,
			NULL,
			0,
			NULL,
			nonce,
			key
	);

	std::string finalOutput = std::string((char*)nonce, 12) + output;

	printf(WARN "Sending encrypted message (12B + %dB + 16B):\n" CLEAR, sz);
	std::cout << "  iv: "; HexDump(std::string(&finalOutput[0], 12));
	std::cout << "data: "; HexDump(std::string(&finalOutput[12], finalOutput.size() - 12 - 16));
	std::cout << " tag: "; HexDump(std::string(&finalOutput[finalOutput.size() - 16], 16));
	std::cout << '\n';

	return finalOutput;
}

std::string DecryptSSL(const char* data, int sz, unsigned char key[32])
{
	std::string plaintext(sz - 12 - 16, 0);
	int outlen;

	unsigned char nonce[12]; memcpy((char*)nonce, data, 12);
	std::string cyphertext = std::string(data, sz).substr(12, sz - 12 - 16); // forgot to add sz
	std::string tag = std::string(data + sz - 16, 16);

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if(!EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, key, nonce)) goto error;
	if(!EVP_DecryptUpdate(ctx, (unsigned char*)&plaintext[0], &outlen, (unsigned char*)&cyphertext[0], cyphertext.size())) goto error;
	if(!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, &tag[0])) goto error;
	if(EVP_DecryptFinal_ex(ctx, (unsigned char*)(plaintext.data() + outlen), &outlen) <= 0) goto error;
	EVP_CIPHER_CTX_free(ctx);

	printf(OK "Decrypted %d bytes successfully.\n" CLEAR, sz - 12 - 16);
	return plaintext;

	error:
	EVP_CIPHER_CTX_free(ctx);
	printf(ERR "Failed to decrypt %d bytes.\n" CLEAR, sz - 12 - 16);
	return "decryptErrorThisIsBad";
}

std::string Decrypt(const char* data, int sz, unsigned char key[32])
{
	std::string plaintext(sz - 12 - 16, 0);
	unsigned long long int outputLen;
	unsigned char nonce[12]; memcpy((char*)nonce, data, 12);

	int err = crypto_aead_aes256gcm_decrypt(
			(unsigned char*)&plaintext[0],
			&outputLen,
			NULL,
			(const unsigned char*)(data + 12),
			sz - 12,
			NULL,
			0,
			nonce,
			key
	);

	if(err)
	{
		printf(ERR "Failed to decrypt %d bytes.\n" CLEAR, sz - 12 - 16);
		return "decryptErrorThisIsBad";
	}

	printf(WARN "Received encrypted message (12B + %dB + 16B):\n" CLEAR, sz);
	std::cout << "   iv: "; HexDump(std::string(data, 12));
	std::cout << "crypt: "; HexDump(std::string(data + 12, sz - 12 - 16));
	std::cout << "  tag: "; HexDump(std::string(data + sz - 16, 16));
	std::cout << "plain: "; std::cout.write(plaintext.data(), plaintext.size()); std::flush(std::cout);
	std::cout << '\n';

	printf(OK "Decrypted %d bytes successfully.\n" CLEAR, sz - 12 - 16);
	return plaintext;
}


/////////////// FILE TRANSMISSION FUNCS ///////////////
void SendFile(const char* filepath, int socket, unsigned char key[32], bool encrypt)
{
	printf(WARN "\n=============== File '%s' send started. ===============\n" CLEAR, filepath);

	// TODO: encrypt filename too

	int fd = open(filepath, O_RDONLY);
	if(fd < 0)
	{
		Packet error(Flags::FAILURE, "File not found on disk!");
		error.Send(socket);
		printf(ERR "File not found on disk! Aborted sending.\n" CLEAR);
		return;
	}

	int size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	std::string data = std::string(basename(filepath)) + "\n" + std::to_string(size) + "\n";
	Packet packet(Flags::SEND_FILE_BEGIN, data);
	packet.Send(socket);

	packet.data = (char*)realloc(packet.data, CHUNK_SIZE + 12 + 16);
	int bytesLeft = size;
	int chunkCount = -1;

	while(bytesLeft > 0)
	{
		packet.flag = Flags::SEND_FILE_CHUNK;

		int readBytes;

		if(encrypt && !DISABLE_CRYPTO)
		{
			readBytes = read(fd, packet.data, CHUNK_SIZE);

			std::string encryptedData = Encrypt(packet.data, readBytes, key);
			memcpy(packet.data, &encryptedData[0], encryptedData.size());

			assert(encryptedData.size() == readBytes + 12 + 16);

			packet.size = encryptedData.size() + 8;
		}
		else
		{
			packet.flag = Flags::SEND_FILE_CHUNK;
			readBytes = read(fd, packet.data, CHUNK_SIZE + 12 + 16);
			packet.size = readBytes + 8;
		}

		packet.Send(socket);
		Packet response;
		response.Recv(socket);

		bytesLeft -= readBytes;
		chunkCount++;

		printf(OK "Sent file chunk %d with size %dB! (%dB/%dB)\n" CLEAR, chunkCount, CHUNK_SIZE, size - bytesLeft, size);
	}

	Packet final(Flags::SEND_FILE_END, NULL, 0);
	final.Send(socket);

	close(fd);
	printf(OK "File '%s' (%dB) was sent succesfully.\n" CLEAR, filepath, size);
}

void RecvFile(const char* filepath, int socket, unsigned char key[32], bool decrypt)
{
	printf(WARN "\n=============== File '%s' recv started. ===============\n" CLEAR, filepath);

	Packet packet(Flags::FILE_REQUEST, filepath, strlen(filepath));
	packet.Send(socket);
	
	packet.Recv(socket);
	if(packet.flag == Flags::FAILURE)
	{
		throw std::runtime_error(packet.DataToStr());
	}

	// FIX: check if filename is taken

	std::string downloadPath = std::string(getenv("HOME")) + "/Downloads/";
	int fd = open( (downloadPath + filepath).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
	int size = 0;
	int chunkCount = -1;

	while(true)
	{
		packet.Recv(socket);

		if(packet.flag == Flags::SEND_FILE_END)
			break;

		std::string chunk;

		if(decrypt && !DISABLE_CRYPTO)
		{
			chunk = Decrypt(packet.data, packet.size - 8, key);
			size += chunk.size();
		}
		else
		{
			chunk = packet.DataToStr();
			size += chunk.size();
		}

		write(fd, chunk.data(), chunk.size());
		chunkCount++;

		Packet response(Flags::SUCCESS, NULL, 0);
		response.Send(socket);

		printf(WARN "Received file chunk %d with size %dB! (%dB)\n" CLEAR, chunkCount, CHUNK_SIZE, size);
	}

	close(fd);
	printf(OK "File '%s' (%dB) was downloaded succesfully.\n" CLEAR, filepath, size);
}
