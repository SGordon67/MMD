#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ncurses.h>
#include <string>
#include <poll.h>
#include <arpa/inet.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int playerNum = -1;

int setUpSockets(int clientSocket, sockaddr_in& serverAddress, struct pollfd (&fds)[1]){
    mvprintw(9, 0, "[DEBUG] ENTERING SOCKET SETUP");
    refresh();
    // specifying address
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    // sending connection request
    connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    // poll setup
    fds[0].fd = clientSocket;
    fds[0].events = POLLIN;

    mvprintw(10, 0, "[DEBUG] Entering loop to get a player number");
    refresh();

    char buffer[1024] = { 0 };
    while(playerNum == -1){
        int ret = poll(fds, 1, 100);
        if(ret > 0){
            int bytes = read(clientSocket, buffer, sizeof(buffer));
            if(bytes > 0){
                mvprintw(11, 0, "[DEBUG] Input detected, proceeding into try block");
                refresh();
                buffer[bytes] = '\0';
                try{
                    std::string jsonStr(buffer, bytes);
                    mvprintw(12, 0, "[DEBUG] after jsonStr, before parse");
                    refresh();
                    auto j = json::parse(jsonStr);
                    auto it = j.find("playerNum");
                    playerNum = *it;
                    // if(it != j.end() && it->is_string()){
                    //     std::string val = *it;
                    //     playerNum = (int)val[0];
                    // }
                } catch(json::parse_error &e){
                    mvprintw(20, 0, "[DEBUG] In catch statement for parsing input");
                    refresh();
                }
            }
        }
    }
    return 0;
};

int main(){
    initscr();
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    refresh();

    // create and set up the sockets
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress;
    struct pollfd fds[1];
    setUpSockets(clientSocket, serverAddress, fds);

    mvprintw(15, 0, "[DEBUG] Sockets setup, moving into game loop");

    char buffer[1024] = { 0 };
    while(true){
        // see if server needs to update
        int ret = poll(fds, 1, 100);
        if(ret > 0){
            // Get input from server
            int bytes = read(clientSocket, buffer, sizeof(buffer));
            if(bytes <= 0){
                nodelay(stdscr, FALSE);
                mvprintw(3, 0, "Server disconnected. Press any key to exit.");
                refresh();
                getch();
                break;
            }
            if(bytes > 0 && bytes < sizeof(buffer)){
                buffer[bytes] = '\0';
                try{
                    // mvprintw(15, 0, "Raw buffer: %s", buffer);
                    json j = json::parse(buffer);
                    int row = 5;
                    mvprintw(row++, 0, "Server Update:\n");
                    for(const auto& client : j["clients"]){
                        int id = client["id"].get<int>();
                        char input = client["last_input"].get<std::string>()[0];
                        mvprintw(row++, 0, "Client %d pressed: %c", id, input);
                        refresh();
                    }
                } catch(json::parse_error &e){
                    mvprintw(13, 0, "[DEBUG] In catch statement for parsing input");
                    refresh();
                }
            }
            // mvprintw(5, 0, "Update from server!");
            // mvprintw(6, 0, "Most Rencent press:");
            // mvprintw(6, 21, "%s", buffer);
            // refresh();
        }
        // getting input
        std::string message;
        int input = getch();
        if(input != ERR){
            mvprintw(0, 0, "You Pressed:");
            mvprintw(0, 13, "%c", char(input));
            mvprintw(1, 0, "Input--Detected");
            refresh();
            // construct json to send to server based on input
            json j;
            j["type"] = "keypress";
            j["input"] = std::string(1, input);
            std::string message = j.dump();
            send(clientSocket, message.c_str(), message.size(), 0);
            // message = std::string(1, input);
            // send(clientSocket, message.c_str(), message.size(), 0);
        }else{
            mvprintw(1, 0, "----Waiting----");
            refresh();
        }
    }

    // closing socket
    endwin();
    close(clientSocket);
}











