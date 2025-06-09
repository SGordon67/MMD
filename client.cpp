#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <string>
#include <poll.h>
#include <arpa/inet.h>

int main(){
	initscr();
	cbreak();
	noecho();
	scrollok(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	refresh();

	// creating socket
	int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

	// specifying address
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

	// sending connection request
	connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

	// poll setup
	struct pollfd fds[1];
	fds[0].fd = clientSocket;
	fds[0].events = POLLIN;

	char buffer[1024] = { 0 };
	while(true){
		// see if server needs to update
		int ret = poll(fds, 1, 100);
		if(ret > 0){
			// Get input from server
			int bytes = read(clientSocket, buffer, sizeof(buffer));
			buffer[bytes] = '\0';
			if(bytes > 0){
				mvprintw(5, 0, "Update from server!");
				mvprintw(6, 0, "Most Rencent press:");
				mvprintw(6, 26, "%s", buffer);
				refresh();
			}
		}
		// getting input
		std::string message;
		int input = getch();
		if(input != ERR){
			mvprintw(0, 0, "You Pressed:");
			mvprintw(0, 13, "%c", char(input));
			mvprintw(10, 0, "Input--Detected");
			refresh();
			message = std::string(1, input);
			send(clientSocket, message.c_str(), message.size(), 0);
		}else{
			mvprintw(10, 0, "----Waiting----");
			refresh();
		}
	}

	// closing socket
	endwin();
	close(clientSocket);
}











