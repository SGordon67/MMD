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

int main(){
	// delay for game updates
	auto updateDelay = std::chrono::seconds(3);
	// Start the timer for sending updates to the client
	auto last_sent = std::chrono::steady_clock::now();
	
	// Make sure to keep the server open until at lease one client has connected.
	bool anyClientEverConnected = false;

	// create a vector of buffers for each of the clients
	std::vector<std::array<char, 1024>> buffers;

	// create socket
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket < 0){
		// endwin();
		perror("socket creation failed");
		return 1;
	}

	// specifying address
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	// binding the socket
	if(bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) > 0){
		// endwin();
		perror("Bind failed");
		close(serverSocket);
		return 1;
	}

	// listening to the assigned socket
	if(listen(serverSocket, 5) < 0){
		// endwin();
		perror("Listen failed");
		close(serverSocket);
		return 1;
	}

	// set up a vector to handle multiple clients
	std::vector<pollfd> mySockets;
	mySockets.push_back({serverSocket, POLLIN, 0});

	while(true){
		int ret = poll(mySockets.data(), mySockets.size(), 100);
		if(ret > 0){
			// check for a new connection
			if(mySockets[0].revents & POLLIN){
				int newClient = accept(serverSocket, nullptr, nullptr);
				if(newClient >= 0){
					mySockets.push_back({newClient, POLLIN, 0});
					buffers.push_back({});
					anyClientEverConnected = true;
				}
			}
			for(int i = mySockets.size() - 1; i >= 1; i--){
				// detect hangup or disconnect error
				if(mySockets[i].revents & (POLLHUP | POLLERR)){
					std::cout << "[INFO] Client disconnected (poll flags): " << mySockets[i].fd << "\n";
					close(mySockets[i].fd);
					mySockets.erase(mySockets.begin() + i);
					buffers.erase(buffers.begin() + i);
					continue;
				}
				// check for readable input
				if(mySockets[i].revents & POLLIN){
					// Get input from client
					int bytes = read(mySockets[i].fd, buffers[i].data(), buffers[i].size() - 1);
					if(bytes <= 0){
						std::cout << "[INFO] Client disconnected (read failure): " << mySockets[i].fd << "\n";
						close(mySockets[i].fd);
						mySockets.erase(mySockets.begin() + i);
						buffers.erase(buffers.begin() + i);
						continue;
					}
					buffers[i][bytes] = '\0';
					std::cout << "[INFO] Client " << i << " Pressed " << buffers[i].data() << "\n";
				}
			}
		}
		// close the server if no clients remain
		if(anyClientEverConnected && mySockets.size() == 1){
			std::cout << "[INFO] All clients disconnected. Shutting down server.\n";
			break;
		}
		auto now = std::chrono::steady_clock::now();
		if(now - last_sent >= updateDelay){
			for(int i = 1; i < mySockets.size(); i++){
				send(mySockets[i].fd, buffers[i].data(), sizeof(buffers[i]), 0);
			}
			last_sent = now;
		}
	}

	close(serverSocket);
	return 0;
}










