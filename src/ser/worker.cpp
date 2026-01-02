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
#include <string>
#include <sstream>
#include <unordered_map>

#include <sodium.h>
#include <tinyxml2.h>
using namespace tinyxml2;

#include "utils.h"
#include "packet.h"
#include "server_state.h"

struct ClientInfo
{
	unsigned char publicKey[32];
	unsigned char privateKey[32];
	unsigned char secretKey[32];

	bool auth = false;

	std::string userDir;
	std::string diskFilename;
	std::string encryptedFilename;
	std::string filenameEncrypted;
	int writeFd;
	int currentFileSz;
	int chunkCount = 0;
	int writeFileSz = 0;

	XMLDocument* userFilesXML = nullptr;
	XMLElement* cwdXML = nullptr;
};

void* ServerWorker(void* arg)
{
	pthread_detach(pthread_self());
	ThreadInfo& threadInfo = *(ThreadInfo*)arg;
	threadInfo.alive = true;

	Packet packet;
	std::unordered_map<int, ClientInfo> clientsInfo;

	fd_set actfds;
	fd_set readfds;
	int nfds;

	FD_ZERO(&actfds);
	FD_SET(glb.listenSocket, &actfds);
	nfds = glb.listenSocket;

	int clientsOnThisThread = 0;

	while(true)
	{
		memcpy(&readfds, &actfds, sizeof(fd_set));

		if(select(nfds + 1, &readfds, NULL, NULL, NULL) < 0)
		{
			PrintErr( ("Thread " + std::to_string(threadInfo.id) + "select").c_str() );
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
				PrintErr( ("Thread " + std::to_string(threadInfo.id) + "accept").c_str() );
				close(glb.listenSocket);
				goto threadDead;
			}
			else
			{
				printf(WARN "\nThread %d: Client %d connected from %s:%d.\n" CLEAR, threadInfo.id,
						clientSocket, inet_ntoa({clientAddr.sin_addr.s_addr}), clientAddr.sin_port);

				// clientsInfo.emplace(clientSocket, ClientInfo());
				clientsInfo[clientSocket] = ClientInfo();

				assert(clientsInfo[clientSocket].userFilesXML == nullptr);

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
				goto killClient;
			}

			printf(OK "Thread %d: Received %d bytes from client %d. (%s)\n" CLEAR, threadInfo.id, packet.size, clientSocket, FlagToStr((Flags)packet.flag).c_str());

			switch(packet.flag)
			{
				case Flags::KEY_EXCHANGE:
				{
					RandomBytes(info.privateKey, 32);

					crypto_scalarmult_curve25519_base(info.publicKey, info.privateKey);

					unsigned char peerPublicKey[32];
					memcpy((char*)peerPublicKey, packet.data, 32);

					int err = crypto_scalarmult_curve25519(info.secretKey, info.privateKey, peerPublicKey);
					if(err)
					{
						printf(ERR "Key exchange failed!\n" CLEAR);
						goto killClient;
					}

					Packet response(Flags::KEY_EXCHANGE, (char*)info.publicKey, 32);
					response.Send(clientSocket);

					printf(OK "Key exchange finished successfully.\n" CLEAR);
					std::cout << "private key: "; HexDump((char*)info.privateKey, 32);
					std::cout << " public key: "; HexDump((char*)info.publicKey, 32);
					std::cout << "   peer key: "; HexDump((char*)peerPublicKey, 32);
					std::cout << " secret key: "; HexDump((char*)info.secretKey, 32);
					std::cout << '\n';
				}
				break;

				case Flags::REGISTER_REQUEST:
				{
					std::string data = DecryptSSL(packet.data, packet.size - 8, info.secretKey);
					std::stringstream stream;
					stream << data;

					std::string username; 
					std::getline(stream, username, '\n');
					std::string password;
					std::getline(stream, password, '\n');

					pthread_mutex_lock(&glb.usersXMLMut);
					XMLElement* user = glb.usersXML.FirstChildElement("users")->FirstChildElement("user");
					pthread_mutex_unlock(&glb.usersXMLMut);
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
						Packet response(Flags::FAILURE, "User already exists");
						response.Send(clientSocket);
						info.auth = false;
					}
					else
					{
						unsigned char hash  [64];
						unsigned char salt_p[32]; // salt is 32B not 16B
						unsigned char salt_e[32];

						RandomBytes(salt_p, 32);
						RandomBytes(salt_e, 32);

						int err = crypto_pwhash_scryptsalsa208sha256(
								hash,
								64,
								&password[0],
								password.size(),
								salt_p,
								crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE,
								crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE
						);

						if(err)
						{
							printf(ERR "libsodium: scrypt failed to hash!\n" CLEAR);
							goto killClient;
						}

						pthread_mutex_lock(&glb.usersXMLMut);
						XMLElement* users = glb.usersXML.FirstChildElement("users");
						pthread_mutex_unlock(&glb.usersXMLMut);
						int count = users->ChildElementCount("user");

						XMLElement* user = users->InsertNewChildElement("user");
						user->SetAttribute("id", count);
						user->InsertNewChildElement("username")->SetText(username.c_str());
						user->InsertNewChildElement("hash")    ->SetText(ToHexString((char*)hash,   64).c_str());
						user->InsertNewChildElement("salt-p")  ->SetText(ToHexString((char*)salt_p, 32).c_str());
						user->InsertNewChildElement("salt-e")  ->SetText(ToHexString((char*)salt_e, 32).c_str());

						info.userDir = ("usr/" + username + "/");

						if(mkdir(info.userDir.c_str(), 0775))
						{
							PrintErr("mkdir");
							goto killClient;
						}

						info.userFilesXML = new XMLDocument();
						XMLElement* root = info.userFilesXML->NewElement("files");
						root->SetAttribute("counter", 0);
						info.userFilesXML->InsertEndChild(root);
						info.userFilesXML->SaveFile((info.userDir + "files.xml").c_str());

						info.cwdXML = info.userFilesXML->FirstChildElement("files");
						assert(info.cwdXML != nullptr);

						Packet response(Flags::SUCCESS, (char*)salt_e, 32);
						response.Send(clientSocket);
						info.auth = true;
					}
				}
				break;

				case Flags::LOGIN_REQUEST:
				{
					std::string data = DecryptSSL(packet.data, packet.size - 8, info.secretKey);
					std::stringstream stream;
					stream << data;

					std::string username; 
					std::getline(stream, username, '\n');
					std::string password;
					std::getline(stream, password, '\n');

					pthread_mutex_lock(&glb.usersXMLMut);
					XMLElement* user = glb.usersXML.FirstChildElement("users")->FirstChildElement("user");
					pthread_mutex_unlock(&glb.usersXMLMut);

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

						if(err)
						{
							printf(ERR "libsodium: scrypt failed to hash!\n" CLEAR);
							goto killClient;
						}

						if(memcmp(hash, oldhash, 64) == 0)
						{
							unsigned char salt_e[32];
							memcpy((char*)salt_e, FromHexString(user->FirstChildElement("salt-e")->GetText(), 64).c_str(), 32);

							info.userDir = ("usr/" + username + "/");

							info.userFilesXML = new XMLDocument();
							info.userFilesXML->LoadFile( (info.userDir + "files.xml").c_str() );

							info.cwdXML = info.userFilesXML->RootElement();
							if(info.cwdXML == nullptr)
							{
								info.cwdXML = info.userFilesXML->NewElement("files");
								info.userFilesXML->InsertEndChild(info.cwdXML);
							}

							assert(info.cwdXML != nullptr);

							Packet response(Flags::SUCCESS, (char*)salt_e, 32);
							response.Send(clientSocket);
							info.auth = true;
							break;
						}
					}

					Packet response(Flags::FAILURE, "Invalid credentials");
					response.Send(clientSocket);
				}
				break;

				case Flags::SEND_FILE_BEGIN: // FIX: ask for overwrite confirmation
				{
					if(info.auth == false)
						break;

					std::stringstream stream;
					stream << packet.DataToStr();

					stream >> info.encryptedFilename;
					stream >> info.currentFileSz;

					XMLElement* file;
					bool found = false;

					assert(info.cwdXML != nullptr);
					file = info.cwdXML->FirstChildElement("file");

					while(file)
					{
						if(info.encryptedFilename == file->FirstChildElement("name")->GetText())
						{
							found = true;
							break;
						}

						file = file->NextSiblingElement("file");
					}

					if(found == false) // create
					{
						info.diskFilename = info.userFilesXML->FirstChildElement("files")->Attribute("counter");
						info.userFilesXML->FirstChildElement("files")->SetAttribute("counter", std::stoi(info.diskFilename) + 1);

						file = info.cwdXML->InsertNewChildElement("file");
						file->SetAttribute("id", info.diskFilename.c_str());

						file->InsertNewChildElement("name")->SetText(info.encryptedFilename.c_str());
						file->InsertNewChildElement("size")->SetText(info.currentFileSz);
						file->InsertNewChildElement("birth")->SetText(time(NULL));
					}
					else // overwrite
					{
						info.diskFilename = file->Attribute("id");

						file->FirstChildElement("name")->SetText(info.encryptedFilename.c_str());
						file->FirstChildElement("size")->SetText(info.currentFileSz);
						file->FirstChildElement("birth")->SetText(time(NULL));
					}


					printf(WARN "\n=============== File '%s' recv started. ===============\n" CLEAR, (info.userDir + info.diskFilename).c_str());
					info.writeFd = open( (info.userDir + info.diskFilename).c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600 ); // FIX: use an id for filename instead
				}
				break;

				case Flags::SEND_FILE_CHUNK:
				{
					if(info.auth == false)
						break;

					printf(WARN "Received file chunk %d with size %dB! (%dB/%dB)\n" CLEAR, info.chunkCount, packet.GetDataSize(), info.writeFileSz, info.currentFileSz);
					int bytes = write(info.writeFd, packet.data, packet.size - 8);

					info.chunkCount++;
					info.writeFileSz += bytes;

					Packet response(Flags::SUCCESS, NULL, 0);
					response.Send(clientSocket);
				}
				break;

				case Flags::SEND_FILE_END:
				{
					if(info.auth == false)
						break;

					printf(OK "File '%s' (%dB) was downloaded succesfully.\n" CLEAR, (info.userDir + info.diskFilename).c_str(), info.currentFileSz);
					close(info.writeFd);
				}
				break;

				case Flags::DIR_LIST_REQUEST:
				{
					if(info.auth == false)
						break;

					if(packet.size == 8)
					{
						assert(info.cwdXML != nullptr);

						XMLElement* file = info.cwdXML->FirstChildElement("file");
						std::stringstream stream;

						while(file)
						{
							std::string name = file->FirstChildElement("name")->GetText();

							stream << false << '\n' << name << '\n';
							file = file->NextSiblingElement("file");
						}

						XMLElement* dir = info.cwdXML->FirstChildElement("dir");

						while(dir)
						{
							std::string name = dir->FirstChildElement("name")->GetText();

							stream << true << '\n' <<  name << '\n';
							dir = dir->NextSiblingElement("dir");
						}

						if(stream.str().size() != 0)
						{
							Packet response(Flags::SUCCESS, stream.str().data(), stream.str().size());
							response.Send(clientSocket);
						}
						else
						{
							Packet response(Flags::SUCCESS, nullptr, 0);
							response.Send(clientSocket);
						}
					}
				}
				break;

				case Flags::SEND_FILE_REQUEST:
				{
					if(info.auth == false)
						break;

					// FIX: refuse to send dirs

					std::string wantEncryptedFile = std::string(packet.data, packet.size - 8);

					XMLElement* file;
					bool found = false;

					assert(info.cwdXML != nullptr);
					file = info.cwdXML->FirstChildElement("file");

					while(file)
					{
						if(wantEncryptedFile == file->FirstChildElement("name")->GetText())
						{
							found = true;
							break;
						}

						file = file->NextSiblingElement("file");
					}

					if(found == false)
					{
						Packet error(Flags::FAILURE, "File not found in database!");
						error.Send(clientSocket);
						printf(ERR "File not found in database! Aborted sending.\n" CLEAR);
						break;
					}

					std::string realFilename = file->Attribute("id");
					std::string filepath = info.userDir + realFilename;
					SendFile(filepath.c_str(), clientSocket, (unsigned char*)"", false);
				}
				break;

				case Flags::CREATE_DIR:
				{
					if(info.auth == false)
						break;

					std::string dirname = packet.DataToStr();

					if(dirname.size() == 0)
					{
						Packet response(Flags::FAILURE, "Invalid directory name");
						response.Send(clientSocket);
						break;
					}

					XMLElement* dir = info.cwdXML->FirstChildElement("dir");
					bool found = false;

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

					XMLElement* newdir = info.cwdXML->InsertNewChildElement("dir");
					newdir->InsertNewChildElement("name")->SetText(dirname.c_str());

					// FIX: check for being unable to create

					Packet response(Flags::SUCCESS, NULL, 0);
					response.Send(clientSocket);
				}
				break;

				case Flags::CHANGE_CWD:
				{
					if(info.auth == false)
						break;

					std::string dirname = packet.DataToStr();

					if(dirname == "../")
					{
						if(info.cwdXML->Parent()->ToElement() != nullptr)
						{
							info.cwdXML = info.cwdXML->Parent()->ToElement();

							Packet response(Flags::SUCCESS, NULL, 0);
							response.Send(clientSocket);
							break;
						}

						Packet response(Flags::FAILURE, "You do not have permission");
						response.Send(clientSocket);
						break;
					}

					XMLElement* dir = info.cwdXML->FirstChildElement("dir");

					while(dir)
					{
						if(dir->FirstChildElement("name")->GetText() == dirname)
						{
							info.cwdXML = dir;
							break;
						}

						dir = dir->NextSiblingElement("dir");
					}

					Packet response(Flags::SUCCESS, NULL, 0);
					response.Send(clientSocket);
				}
				break;

				case Flags::LOGOUT:
				{
					if(info.userFilesXML != nullptr)
					{
						info.userFilesXML->SaveFile( (info.userDir + "files.xml").c_str() );
						delete info.userFilesXML;
						info.userFilesXML = nullptr;
					}

					info.auth = false;
				}
				break;

				case Flags::FILE_DELETE:
				{
					// TODO:
				}
				break;

				case Flags::FILE_RENAME:
				{
					// TODO:
				}
				break;

				case Flags::FILE_COPY:
				{
					// TODO:
				}
				break;

				case Flags::FILE_MOVE:
				{
					// TODO:
				}
				break;

				case Flags::QUIT:
				{
					goto killClient;
				}
				break;
			}

			continue;

			killClient:
			printf(WARN "\nClient %d disconnected.\n\n" CLEAR, clientSocket);

			pthread_mutex_lock(&glb.miscMut);
			glb.clientCount--;
			pthread_mutex_unlock(&glb.miscMut);
			clientsOnThisThread--;

			if(info.userFilesXML != nullptr)
			{
				info.userFilesXML->SaveFile( (info.userDir + "files.xml").c_str() );
				delete info.userFilesXML;
			}
			// clientsInfo.erase(clientSocket);

			// if(info.fileOpen == true)
			// 	close(info.fd);

			FD_CLR(clientSocket, &actfds);
			close(clientSocket);
		}

		// std::cout << "Thread " << threadInfo.id << " working..." << "\n";
	}

	return NULL;

	threadDead:
	printf(ERR "Thread %d died!\n" CLEAR, threadInfo.id);

	pthread_mutex_lock(&glb.miscMut);
	glb.clientCount -= clientsInfo.size();
	pthread_mutex_unlock(&glb.miscMut);

	threadInfo.alive = false;
	free(arg);
	return NULL;
}
