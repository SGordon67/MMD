#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <string>
#include <chrono>
#include <poll.h>
#include <vector>
#include <array>

// TODO allow for multiple clients
int main(){
	initscr();
	cbreak();
	noecho();
	scrollok(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	refresh();
	
	// delay for game updates
	auto updateDelay = std::chrono::seconds(3);

	// Start the timer for sending updates to the client
	auto last_sent = std::chrono::steady_clock::now();
	
	// create a vector of buffers for each of the clients
	std::vector<std::array<char, 1024>> buffers;

	// create socket
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket < 0){
		endwin();
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
		endwin();
		perror("Bind failed");
		close(serverSocket);
		return 1;
	}

	// listening to the assigned socket
	if(listen(serverSocket, 5) < 0){
		endwin();
		perror("Listen failed");
		close(serverSocket);
		return 1;
	}

	// set up a vector to handle multiple clients
	std::vector<pollfd> mySockets;
	mySockets.push_back({serverSocket, POLLIN, 0});
	// accept clients
	while(true){
		int ret = poll(mySockets.data(), mySockets.size(), 100);
		if(ret < 0){
			perror("Poll failed");
			break;
		}

		// check for a new connection
		if(mySockets[0].revents & POLLIN){
			int newClient = accept(serverSocket, nullptr, nullptr);
			if(newClient >= 0){
				mySockets.push_back({newClient, POLLIN, 0});
				buffers.push_back({});
				mvprintw(0, 0, "New client connected: %d", newClient);
				refresh();
			}
		}

		// if there are two players then continue on
		if(mySockets.size() > 2) break;
	}

	while(true){
		for(int i = 1; i < mySockets.size(); i++){
			int ret = poll(mySockets.data(), mySockets.size(), 100);
			if(ret > 0){
				if(mySockets[i].revents & POLLIN){
					// mvprintw(10, 0, "TESTING IF WE CAN EVEN GET INPUT");
					// Get input from client
					int bytes = read(mySockets[i].fd, buffers[i].data(), buffers[i].size() - 1);
					buffers[i][bytes] = '\0';
					if(bytes > 0){
						mvprintw(i+4, 0, "Client pressed:");
						mvprintw(i+4, 16, "%s", buffers[i].data());
						refresh();
					}
				}
			}
		}
		auto now = std::chrono::steady_clock::now();
		if(now - last_sent >= updateDelay){
			for(int i = 1; i < mySockets.size(); i++){
				send(mySockets[i].fd, buffers[i].data(), sizeof(buffers[i]), 0);
			}
			last_sent = now;
		}
	}

	endwin();
	close(serverSocket);
}




















