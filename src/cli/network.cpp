#include "network.h"

#include "client_state.h"
#include "utils.h"
#include "packet.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>

#include <sodium.h>

#include <stdlib.h>

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

void SigInt_Handler(int sig)
{
	Packet packet(Flags::QUIT, NULL, 0);
	packet.Send(glb.serverSocket);

	printf(WARN "\nDisconnected forcefully.\n" CLEAR);
	close(glb.serverSocket);
	exit(1);
}

void SigPipe_Handler(int sig)
{
	printf("SIGPIPE\n");
}

void Connect()
{
	glb.serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(glb.serverSocket < 0)
	{
		PrintErr("socket");
		exit(1);
	}

	signal(SIGINT, SigInt_Handler);
	signal(SIGPIPE, SigPipe_Handler);

	sockaddr_in serverAddr;
	memset(&serverAddr, 0, sizeof(serverAddr));

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(glb.ip);
	serverAddr.sin_port = htons((short)atoi(glb.port));

	if(connect(glb.serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)))
	{
		// TODO: retry connection
		PrintErr("connect");
		glb.error = true;
		glb.errormsg = std::string(strerror(errno));
		return;
	}
	else
	{
		glb.connected = true;
		glb.error = false;
		printf(WARN "Connected to server.\n" CLEAR);
	}

	// KEY EXCHANGE
	unsigned char private_key[32];
	unsigned char public_key[32];

	int fd = open("/dev/random", O_RDONLY);
	read(fd, private_key, 32);
	close(fd);

	crypto_scalarmult_curve25519_base(public_key, private_key);
	std::cout << "private key: "; HexDump((char*)private_key, 32);
	std::cout << "public  key: "; HexDump((char*)public_key,  32);

	Packet packet(Flags::KEY_EXCHANGE, (char*)public_key, 32);
	packet.Send(glb.serverSocket);
	packet.Recv(glb.serverSocket);

	if(packet.flag == Flags::KEY_EXCHANGE)
	{
		unsigned char peer_public_key[32];
		memcpy((char*)peer_public_key, packet.data, 32);
		std::cout << "peer   key: "; HexDump((char*)peer_public_key, 32);
		int err = crypto_scalarmult_curve25519(glb.secret_key, private_key, peer_public_key);
		// err
		std::cout << "secret key: "; HexDump((char*)glb.secret_key, 32);
	}
	else
	{
		// refused
	}

	// TODO: send a known encrypted message to make sure the key exchange actually worked..?
	printf(OK "KEY EXCHANGE SUCCESS\n" CLEAR);
}

// FIX: crashes sometimes on login
int SendAuthReq(Flags flag)
{
	std::string data = std::string(glb.username) + '\n' + glb.password + '\n';
	std::cout << "expect enc msg len: " << data.size() + 12 + 16 << '\n';
	std::string encrypted_data = Encrypt(data.c_str(), data.size(), glb.secret_key);

	std::cout << "OK LETS CHECK AGAIN WHAT WE ARE SENDING TO SERVER:\n";
	std::cout << "AES MESSAGE:\n";
	std::cout << "nonce     : "; HexDump(std::string(&encrypted_data[0], 12));
	std::cout << "cyphertext: "; HexDump(std::string(&encrypted_data[12], encrypted_data.size() - 12 - 16));
	std::cout << "MAC       : "; HexDump(std::string(&encrypted_data[encrypted_data.size() - 16], 16));

	Packet packet(flag, &encrypted_data[0], encrypted_data.size());
	try
	{
		packet.Send(glb.serverSocket);
		packet.Recv(glb.serverSocket);
	}
	catch(std::exception& e)
	{
		return -1;
	}

	if(packet.flag == Flags::FAILURE)
	{
		printf(ERR "AUTH FAIL\n" CLEAR);
		return -1;
	}

	memcpy(glb.salt_e, packet.data, 32);
	HexDump((char*)glb.salt_e, 32);

	printf(OK "AUTH SUCCESS\n" CLEAR);

	int err = crypto_pwhash_argon2id( // FIX:
			glb.fileKey,
			32,
			glb.password,
			strlen(glb.password),
			glb.salt_e,
			crypto_pwhash_argon2id_OPSLIMIT_INTERACTIVE,
			crypto_pwhash_argon2id_MEMLIMIT_INTERACTIVE,
			crypto_pwhash_argon2id_ALG_ARGON2ID13
	);

	std::cout << "FILE KEY: ";
	HexDump((char*)glb.fileKey, 32);

	UpdateDirListContents();

	return 0;
}

void UpdateDirListContents()
{
	// ask list files 
	Packet packet2(Flags::DIR_LIST_REQUEST, NULL, 0);
	packet2.Send(glb.serverSocket);
	packet2.Recv(glb.serverSocket);

	std::stringstream stream;
	stream.write(packet2.data, packet2.size - 8); // FIX: confusignly named packet2. just make a move ctor

	std::cout << "Received directory listing: " << stream.str() << '\n';
	std::cout << "recv: " << packet2.size << '\n';

	glb.dirContents.clear();
	std::cout << "after clear: " << glb.dirContents.size() << '\n';

	while(!stream.eof())
	{
		std::string filename;
		bool isDir;
		stream >> isDir;
		stream.ignore(1, '\n');
		std::getline(stream, filename, '\n');

		// FIX:
		if(filename.size() == 0) break;
		std::cout << isDir << ' ' << filename << '\n';

		glb.dirContents.push_back(std::make_pair(filename, isDir));
	}

	std::sort(glb.dirContents.begin(), glb.dirContents.end(), [](const auto& leftpair, const auto& rightpair) {

		std::string l = leftpair.first;
		std::string r = rightpair.first;

		if(leftpair.second && !rightpair.second)
			return true;
		if(!leftpair.second && rightpair.second)
			return false;

		return l < r;
	});

	std::cout << "sizeof dircontents: " << glb.dirContents.size() << '\n';
}
