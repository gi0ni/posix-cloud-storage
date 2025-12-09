#include "worker.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>

#include <sodium.h>
#include <tinyxml2.h>

using namespace tinyxml2;

#include "packet.h"
#include "utils.h"
#include "server_state.h"

struct ClientInfo
{
	unsigned char public_key[32];
	unsigned char private_key[32];
	unsigned char secret_key[32];

	bool auth = false;

	std::string userdir;
	std::string filename;
	std::string filenameenc;
	int fd;
	int currfilesize;

	XMLDocument* userFiles = nullptr;
	XMLElement* currDirXML = nullptr;
};

void* ServerWorker(void* arg)
{
	pthread_detach(pthread_self());
	ThreadInfo& threadinfo = *(ThreadInfo*)arg;
	threadinfo.alive = true;

	Packet packet;
	// std::cout << (packet.data == nullptr) << '\n';

	// FIX: what if the xml doc gets destroyed midway through

	std::unordered_map<int, ClientInfo> clientsInfo;

	fd_set actfds;
	fd_set readfds;
	int nfds;

	FD_ZERO(&actfds);
	FD_SET(glb.listenSocket, &actfds);
	nfds = glb.listenSocket;

	int x = 0;
	int clientsOnThisThread = 0;

	while(true)
	{
		memcpy(&readfds, &actfds, sizeof(fd_set));

		timeval tm;
		tm.tv_sec = 5;
		tm.tv_usec = 0;

		if(select(nfds + 1, &readfds, NULL, NULL, NULL) < 0) // FIX: just make it infinite
		{
			PrintErr("select");
			goto threadDead;
		}

		if(FD_ISSET(glb.listenSocket, &readfds) && clientsOnThisThread < MAX_CLIENTS_PER_THREAD && pthread_mutex_trylock(&glb.listenSocketMut) == 0)
		{
			int clientSocket;
			sockaddr_in clientAddr;
			socklen_t addrLength = sizeof(clientAddr);

			clientSocket = accept(glb.listenSocket, (sockaddr*)&clientAddr, &addrLength);
			if(clientSocket < 0)
			{
				PrintErr("accept");
				close(glb.listenSocket);
				goto threadDead;
			}
			else
			{
				printf(OK "Thread %d: " CLEAR, threadinfo.id);
				printf(WARN "\nClient %d connected from %s:%d.\n" CLEAR, clientSocket, inet_ntoa({clientAddr.sin_addr.s_addr}), clientAddr.sin_port);

				clientsInfo.emplace(clientSocket, ClientInfo());

				nfds = std::max(nfds, clientSocket);
				FD_SET(clientSocket, &actfds);

				glb.clientCount++;
				clientsOnThisThread++;
				glb.serverTimeout = 120;
			}

			pthread_mutex_unlock(&glb.listenSocketMut);
		}

		for(auto it = clientsInfo.begin(); it != clientsInfo.end(); it++)
		{
			int clientSocket = it->first;
			ClientInfo& info = it->second;

			if(!FD_ISSET(clientSocket, &readfds))
				continue;

			try
			{
				packet.Recv(clientSocket);
			}
			catch(std::exception& e)
			{
				printf(ERR "Failed to recv from client %d.\n" CLEAR, clientSocket);
				// goto threadDead;
				// goto killclient;
				printf(WARN "Client %d disconnected.\n\n" CLEAR, clientSocket);
				if(info.userFiles != nullptr)
				{
					info.userFiles->SaveFile( (info.userdir + "files.xml").c_str() );
				}
				FD_CLR(clientSocket, &actfds);
				close(clientSocket);
				// pthread_mutex_lock(&glb.mut);
				glb.clientCount -= 1;
				// pthread_mutex_unlock(&glb.mut);
				return NULL;
			}

			printf(OK "Received flag %d from client.\n" CLEAR, packet.flag);

			switch(packet.flag)
			{
				case Flags::KEY_EXCHANGE:
				{
					int fd = open("/dev/random", O_RDONLY);
					read(fd, &info.private_key[0], 32);
					close(fd);

					crypto_scalarmult_curve25519_base(info.public_key, info.private_key);
					std::cout << "private key: "; HexDump((char*)info.private_key, 32);
					std::cout << "public  key: "; HexDump((char*)info.public_key,  32);

					unsigned char peer_public_key[32];
					memcpy((char*)peer_public_key, packet.data, 32);
					std::cout << "peer key: "; HexDump((char*)peer_public_key, 32);

					int err = crypto_scalarmult_curve25519(info.secret_key, info.private_key, peer_public_key);
					std::cout << "secret key: "; HexDump((char*)info.secret_key, 32);

					printf("SEND BACK KEY\n");
					std::cout << "packing: "; HexDump((char*)info.public_key, 32);
					Packet response(Flags::KEY_EXCHANGE, (char*)info.public_key, 32);
					std::cout << "sending: "; HexDump((char*)response.data, 32);
					response.Send(clientSocket);
				}
				break;

				case Flags::REGISTER_REQUEST:
				{
					std::string data = DecryptSSL(packet.data, packet.size - 8, info.secret_key);

					std::stringstream stream;
					stream << data;

					std::string username; 
					std::getline(stream, username, '\n');
					std::string password;
					std::getline(stream, password, '\n');

					XMLElement* user = glb.doc.FirstChildElement("users")->FirstChildElement("user");

					bool found = false;

					while(user)
					{
						std::string u = user->FirstChildElement("username")->GetText();

						if(u == username)
						{
							found = true;
							break;
						}

						user = user->NextSiblingElement("user");
					}

					if(found)
					{
						Packet response(Flags::FAILURE, NULL, 0);
						response.Send(clientSocket);
						info.auth = false;
					}
					else
					{
						unsigned char hash  [64];
						unsigned char salt_p[32]; // WARN: SALT IS 32B NOT 16B
						unsigned char salt_e[32];

						int fd = open("/dev/random", O_RDONLY);
						read(fd, salt_p, 32);
						read(fd, salt_e, 32);
						close(fd);

						int err = crypto_pwhash_scryptsalsa208sha256(
								hash,
								64,
								&password[0],
								password.size(),
								salt_p,
								crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE,
								crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE
								);

						XMLElement* users = glb.doc.FirstChildElement("users");
						int count = users->ChildElementCount("user");

						XMLElement* user = users->InsertNewChildElement("user");
						user->SetAttribute("id", count);
						user->InsertNewChildElement("username")->SetText(username.c_str());
						user->InsertNewChildElement("hash")->SetText(ToHexString((char*)hash, 64).c_str());
						user->InsertNewChildElement("salt-p")->SetText(ToHexString((char*)salt_p, 32).c_str());
						user->InsertNewChildElement("salt-e")->SetText(ToHexString((char*)salt_e, 32).c_str());

						mkdir( ("usr/" + username).c_str(), 0775 );

						// FIX: out of range somewhere in here

						info.userFiles = new XMLDocument();
						XMLElement* files = info.userFiles->NewElement("files");
						info.userFiles->InsertEndChild(files);
						info.userFiles->SaveFile( ("usr/" + username + "/files.xml").c_str() );

						Packet response(Flags::ACCEPT, (char*)salt_e, 32);
						response.Send(clientSocket);

						info.userdir = ("usr/" + username + "/");
						info.currDirXML = info.userFiles->FirstChildElement("files");
						assert(info.currDirXML != nullptr);
						info.auth = true;
					}
				}
				break;

				case Flags::AUTH_REQUEST:
				{
					std::cout << "received: " << packet.size - 8 << '\n';
					std::cout << packet.data << '\n';

					std::string data = DecryptSSL(packet.data, packet.size - 8, info.secret_key);

					std::stringstream stream;
					stream << data;

					std::string username; 
					std::getline(stream, username, '\n');
					std::string password;
					std::getline(stream, password, '\n');

					std::cout << username << '\n';
					std::cout << password << '\n';

					XMLElement* user = glb.doc.FirstChildElement("users")->FirstChildElement("user");

					bool found = false;

					while(user)
					{
						std::string u = user->FirstChildElement("username")->GetText();

						if(u == username)
						{
							found = true;
							break;
						}

						user = user->NextSiblingElement("user");
					}

					if(found)
					{
						unsigned char oldhash[64];
						unsigned char hash[64];
						unsigned char salt_p[32];

						memcpy((char*)oldhash, FromHexString(user->FirstChildElement("hash")->GetText(), 128).c_str(), 64);
						memcpy((char*)salt_p,  FromHexString(user->FirstChildElement("salt-p")->GetText(), 64).c_str(), 32);

						int err = crypto_pwhash_scryptsalsa208sha256(
								hash,
								64,
								password.c_str(),
								password.size(),
								salt_p,
								crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE,
								crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE
								);

						if(memcmp(hash, oldhash, 64) == 0)
						{
							unsigned char salt_e[32];
							memcpy((char*)salt_e, FromHexString(user->FirstChildElement("salt-e")->GetText(), 64).c_str(), 32);

							Packet response(Flags::ACCEPT, (char*)salt_e, 32);
							response.Send(clientSocket);
							info.userdir = ("usr/" + username + "/");
							info.userFiles = new XMLDocument();
							info.userFiles->LoadFile( (info.userdir + "files.xml").c_str() );
							info.auth = true;
							std::cout << info.userFiles->ChildElementCount() << '\n';
							info.currDirXML = info.userFiles->RootElement();
							assert(info.currDirXML != nullptr);
							break;
						}
					}

					Packet response(Flags::FAILURE, NULL, 0);
					response.Send(clientSocket);

					info.auth = false;
				}
				break;

				case Flags::SEND_FILE_BEGIN: // FIX: if file exists then ask for confirmation to overwrite
				{
					std::stringstream stream;
					stream.write(packet.data, packet.size - 8);

					stream >> info.filename;
					stream >> info.currfilesize;

					std::cout << "preparing to write " << info.userdir + info.filename << '\n';
					info.fd = open( (info.userdir + info.filename).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600 );
				}
				break;

				case Flags::FILE_CHUNK:
				{
					std::cout << "FILE CHUNK SIZE: " << packet.size - 8 << '\n';
					write(info.fd, packet.data, packet.size - 8);
					Packet response(Flags::ACCEPT, NULL, 0);
					response.Send(clientSocket);
				}
				break;

				case Flags::SEND_FILE_END:
				{
					// check file intact

					// make backup

					XMLElement* file = info.currDirXML->InsertNewChildElement("file");

					// FIX: hash file name

					// FIX: there could be a filename collision. use scrypt with salt.

					file->InsertNewChildElement("name")->SetText(info.filename.c_str());
					file->InsertNewChildElement("size")->SetText(info.currfilesize);
					file->InsertNewChildElement("birth")->SetText(time(NULL));

					std::cout << "FILE TRANSFER COMPLETE!\n";
					close(info.fd); // WARN: do i have to manually add '\n' at end..?
				}
				break;

				case Flags::DIR_LIST_REQUEST:
				{
					if(packet.size == 8)
					{
						assert(info.currDirXML != nullptr);
						XMLElement* file = info.currDirXML->FirstChildElement("file");

						std::stringstream stream;

						std::cout << "Listing for client: \n";

						while(file)
						{
							std::string name = file->FirstChildElement("name")->GetText();

							stream << false << '\n' << name << '\n';
							std::cout << false << ' ' << name << '\n';
							file = file->NextSiblingElement("file");
						}

						XMLElement* dir = info.currDirXML->FirstChildElement("dir");

						while(dir)
						{
							std::string name = dir->FirstChildElement("name")->GetText();

							stream << true << '\n' <<  name << '\n';
							std::cout << true << ' ' << name << '\n';
							dir = dir->NextSiblingElement("dir");
						}

						if(stream.str().size() != 0)
						{
							std::cout << "Sending to client list stream: " << stream.str() << "; stream size is: " << stream.str().size() << '\n';
							Packet response(Flags::DIR_LIST, stream.str().data(), stream.str().size());
							std::cout << "Sending to client list stream: " << std::string(response.data) << "; stream size is: " << response.size << '\n';
							response.Send(clientSocket);
						}
						else
						{
							Packet response(Flags::DIR_LIST, nullptr, 0);
							response.Send(clientSocket);
						}
					}
				}
				break;

				case Flags::FILE_REQUEST:
				{
					// FIX: refuse to send dirs
					std::string filename = std::string(packet.data, packet.size - 8);
					std::string filepath = info.userdir + filename;
					// prepend cwd

					std::cout << "filename: " << filename << '\n';
					SendFile(filepath.c_str(), clientSocket, (unsigned char*)"", false);
				}
				break;

				case Flags::FILE_REMOVE:
				{
					// quit
				}
				break;

				case Flags::CREATE_DIR:
				{
					std::string dirname = std::string(packet.data, packet.size - 8);

					if(dirname.size() == 0)
					{
						Packet response(Flags::FAILURE, "Bad name");
						response.Send(clientSocket);
						break;
					}

					bool found = false;
					XMLElement* dir = info.currDirXML->FirstChildElement("dir");
					while(dir)
					{
						if(dir->FirstChildElement("name")->GetText() == dirname)
						{
							found = true;
							break;
						}

						dir = dir->NextSiblingElement("dir");
					}

					if(found)
					{
						Packet response(Flags::FAILURE, "Already exists");
						response.Send(clientSocket);
						break;
					}

					XMLElement* newdir = info.currDirXML->InsertNewChildElement("dir");
					newdir->InsertNewChildElement("name")->SetText(dirname.c_str());

					// FIX: check for being unable to create

					Packet response(Flags::ACCEPT, NULL, 0);
					response.Send(clientSocket);
				}
				break;

				case Flags::CHANGE_DIR:
				{
					std::string dirname(packet.data, packet.size - 8);

					if(dirname == "../")
					{
						if(info.currDirXML->Parent()->ToElement() != nullptr)
						{
							info.currDirXML = info.currDirXML->Parent()->ToElement();
							Packet response(Flags::ACCEPT, NULL, 0);
							response.Send(clientSocket);
							break;
						}

						Packet response(Flags::FAILURE, "Can't go out of your root!");
						response.Send(clientSocket);
						break;
					}

					XMLElement* dir = info.currDirXML->FirstChildElement("dir");

					while(dir)
					{
						if(dir->FirstChildElement("name")->GetText() == dirname)
						{
							info.currDirXML = dir;
							break;
						}

						dir = dir->NextSiblingElement("dir");
					}

					Packet response(Flags::ACCEPT, NULL, 0);
					response.Send(clientSocket);
				}
				break;

				case Flags::QUIT:
				{
					goto killclient;
				}
				break;
			}

			continue;

			killclient:
			printf(WARN "Client %d disconnected.\n\n" CLEAR, clientSocket);
			if(info.userFiles != nullptr)
			{
				info.userFiles->SaveFile( (info.userdir + "files.xml").c_str() );
			}
			FD_CLR(clientSocket, &actfds);
			close(clientSocket);
			// pthread_mutex_lock(&glb.mut);
			glb.clientCount -= 1;
			// pthread_mutex_unlock(&glb.mut);
			delete info.userFiles;
			clientsOnThisThread--;
		}

		std::cout << "Thread " << threadinfo.id << ": poll!" << x++ << "\n";

		if(x > 10000)
			goto threadDead;
	}

	return NULL;

	threadDead:
	glb.clientCount -= clientsInfo.size();
	free(arg);
	threadinfo.alive = false;
	printf(ERR "Thread %d died!\n" CLEAR, threadinfo.id);
	return NULL;
}
