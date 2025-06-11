// TODO
// 1. make server -> client communication into JSON
//		^ currently broken even without json
// 2. add keypress info to JSON structure
// 3. client parse JSON from server
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <chrono>
#include <poll.h>
#include <vector>
#include <array>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ClientInfo{
	int fd;
	std::array<char, 1024> buffer;
};

int main(){
	// delay for game updates
	auto updateDelay = std::chrono::seconds(3);
	auto last_sent = std::chrono::steady_clock::now();

	// Make sure to keep the server open until at lease one client has connected.
	bool anyClientEverConnected = false;

	// create a vector of buffers for each of the clients
	std::vector<ClientInfo> clients;

	// create server socket
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket < 0){
		perror("socket creation failed");
		return 1;
	}

	int opt = 1;
	setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// specifying address
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	// binding the socket
	if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) > 0){
		perror("Bind failed");
		close(serverSocket);
		return 1;
	}

	// listening to the assigned socket
	if(listen(serverSocket, 5) < 0){
		perror("Listen failed");
		close(serverSocket);
		return 1;
	}

	while(true){
		std::vector<pollfd> pollFds;
		pollFds.push_back({serverSocket, POLLIN, 0});
		for(const auto& client : clients){
			pollFds.push_back({client.fd, POLLIN, 0});
		}
		int ret = poll(pollFds.data(), pollFds.size(), 100);
		if(ret > 0){
			// check for a new connection
			if(pollFds[0].revents & POLLIN){
				int newClient = accept(serverSocket, nullptr, nullptr);
				if(newClient >= 0){
					pollFds.push_back({newClient, POLLIN, 0});
					clients.push_back({newClient, {}});
					anyClientEverConnected = true;
				}
			}
			for(int i = pollFds.size() - 1; i >= 1; i--){
				// detect hangup or disconnect error
				if(pollFds[i].revents & (POLLHUP | POLLERR)){
					std::cout << "[INFO] Client disconnected (poll flags): " << pollFds[i].fd << "\n";
					close(pollFds[i].fd);
					pollFds.erase(pollFds.begin() + i);
					clients.erase(clients.begin() + (i-1));
					continue;
				}
				// check for readable input
				if(pollFds[i].revents & POLLIN){
					// Get input from client
					int clientIndex = i-1;
					ClientInfo client = clients[clientIndex];
					int bytes = read(client.fd, client.buffer.data(), client.buffer.size() - 1);
					if(bytes <= 0){
						std::cout << "[INFO] Client disconnected (read failure): " << client.fd << "\n";
						close(client.fd);
						pollFds.erase(pollFds.begin() + i);
						clients.erase(clients.begin() + clientIndex);
						continue;
					}
					if(bytes > 0 && bytes < client.buffer.size()){
						// std::cout << "[DEBUG] Received raw: '" << client.buffer.data() << "'\n";
						try{
							std::string jsonStr(client.buffer.data(), bytes);
							auto j = json::parse(jsonStr);
							auto it = j.find("value");
							if(it != j.end() && it->is_string()){
								std::string val = *it;
								std::cout << "[INFO] Client " << i << " Pressed " << val << "\n";
							} else{
								std::cout << "[WARNING] Unkown message format.\n";
							}
						} catch(json::parse_error &e){
							std::cout << "[DEBUG] In catch statement for parsing input\n";
							std::cerr << "JSON parse error: " << e.what() << "\n";
						}
					}
					// std::cout << "[INFO] Client " << i << " Pressed " << buffers[i].data() << "\n";
				}
			}
		}
		// close the server if no clients remain
		if(anyClientEverConnected && clients.empty() == 1){
			std::cout << "[INFO] All clients disconnected. Shutting down server.\n";
			break;
		}
		auto now = std::chrono::steady_clock::now();
		if(now - last_sent >= updateDelay){
			for(auto& client : clients){
				send(client.fd, client.buffer.data(), client.buffer.size(), 0);
				// send(mySockets[i].fd, buffers[i].data(), sizeof(buffers[i]), 0);
			}
			last_sent = now;
		}
	}
	close(serverSocket);
	return 0;
}










