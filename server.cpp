#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <string>
#include <chrono>
#include <poll.h>

int main(){
	initscr();
	cbreak();
	noecho();
	scrollok(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	refresh();
	
	// delay for game updates
	auto updateDelay = std::chrono::seconds(5);

	// create socket
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	// specifying address
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	// binding the socket
	bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

	// listening to the assigned socket
	listen(serverSocket, 5);

	// accepting conection request
	int clientSocket = accept(serverSocket, nullptr, nullptr);

	// Start the timer for sending updates to the client
	auto last_sent = std::chrono::steady_clock::now();

	// poll setup
	struct pollfd fds[1];
	fds[0].fd = clientSocket;
	fds[0].events = POLLIN;

	char buffer[1024] = { 0 };
	while(true){
		// see if server needs to update
		int ret = poll(fds, 1, 100);
		if(ret > 0){
			// Get input from client
			int bytes = read(clientSocket, buffer, sizeof(buffer));
			buffer[bytes] = '\0';
			if(bytes > 0){
				mvprintw(0, 0, "Client pressed:");
				mvprintw(1, 0, "%s", buffer);
				refresh();
			}
		}

		auto now = std::chrono::steady_clock::now();
		if(now - last_sent >= updateDelay){
			send(clientSocket, buffer, sizeof(buffer), 0);
			last_sent = now;
		}
	}

	endwin();
	close(serverSocket);
}




















