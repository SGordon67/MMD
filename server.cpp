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
#include <set>

using json = nlohmann::json;

struct ClientInfo{
    int fd;
    char last_input;
};

enum class hexState {player, ground, air, city, destroyed, blank, wall};

char getHexRep(hexState myHex){
    switch(myHex){
        case hexState::air:
            return 'A';
            break;
        case hexState::city:
            return 'C';
            break;
        case hexState::player:
            return 'P';
            break;
        case hexState::wall:
            return 'W';
            break;
        default:
            return '~';
    }
    return '!';
}

struct coord{
    int q;
    int r;
};
enum class Direction{
    SE = 0,
    NE = 1,
    N = 2,
    NW = 3,
    SW = 4,
    S = 5
};
const coord nDiff[2][6] = {
    // even columns
    {{1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1, 0}, {0, 1}},
    // odd columns
    {{1,1}, {1,0}, {0,-1}, {-1,0},{-1,1}, {0,1}}
};

struct Missile{
    int q; // column
    int r; // row
    Direction dir;
    // 0 = southeast
    // 1 = northeast
    // 2 = north
    // 3 = northwest
    // 4 = southwest
    // 5 = south
    Missile(){
    }
    Missile(int q, int r, Direction dir){
        this->q = q;
        this->r = r;
        this->dir = dir;
    }
};

struct Hex{
    int q; // columns
    int r; // rows
    bool hasMissile;
    hexState state;

    bool operator==(const Hex& other) const{
        return (q == other.q && r == other.r);
    }
    Hex(){
        this->q = 0;
        this->r = 0;
        this->hasMissile = false;
        this->state = hexState::blank;
    }
    Hex(int q, int r, bool hasMissile, hexState state){
        this->q = q;
        this->r = r;
        this->hasMissile = hasMissile;
        this->state = state;
    }
};

struct HexGrid{
    int boardWidth;
    int boardHeight;
    std::vector<std::vector<Hex>> GB;

    HexGrid(int w, int h){
        this->boardWidth = w;
        this->boardHeight = h;
    }
    void initializeDefaultBoard(){
        this->GB.resize(boardWidth, std::vector<Hex>(boardHeight));
        for(int y = 0; y < this->boardHeight; y++){
            for(int x = 0; x < this->boardWidth; x++){
                Hex newHex;
                if(x == 0 || x == this->boardWidth - 1){
                    newHex = Hex(x, y, false, hexState::wall);
                }else if(y == this->boardHeight - 1){
                    if(x == boardWidth/2){
                        newHex = Hex(x, y, false, hexState::player);
                    }else{
                        newHex = Hex(x, y, false, hexState::city);
                    }
                }else{
                    newHex = Hex(x, y, false, hexState::air);
                }
                this->GB[x][y] = newHex;
            }
        }
    }
    void printGrid(){
        std::cout << "Game Board:\n";
        for(int y = 0; y < this->boardHeight; y++){
            std::string subRow1 = "";
            std::string subRow2 = "";
            for(int x = 0; x < this->boardWidth; x++){
                char placeMarker = getHexRep(this->GB[x][y].state);
                if(this->GB[x][y].hasMissile) placeMarker = 'M';
                if(!(x%2)){
                    subRow1 += " ";
                    subRow1.push_back(placeMarker);
                    subRow1 += " ";
                    subRow2 += "   ";
                }else{
                    subRow1 += "   ";
                    subRow2 += " ";
                    subRow2.push_back(placeMarker);
                    subRow2 += " ";
                }
            }
            std::cout << subRow1 << std::endl << subRow2 << std::endl;
        }
    }
};

void updateMissiles(std::vector<Missile>& missiles, HexGrid& grid){
    for(int i = 0; i < missiles.size(); i++){
        int q = missiles[i].q;
        int r = missiles[i].r;
        int parity = q % 2;
        Direction dir = missiles[i].dir;
        coord delta = nDiff[parity][static_cast<int>(dir)];

        int newQ = q + delta.q;
        int newR = r + delta.r;

        // update the grid and missile info
        grid.GB[q][r].hasMissile = false;
        grid.GB[newQ][newR].hasMissile = true;
        missiles[i].q = newQ;
        missiles[i].r = newR;

        // std::cout << "Moving from (" << r << ", " << q << ") to (" << newR << ", " << newQ << ")\n";
        return;
    }
}

int main(){
    // delay for game updates
    auto updateDelay = std::chrono::seconds(1);
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

    // game logic and board creation
    const int boardWidth = 17;
    const int boardHeight = 10;

    HexGrid gameBoard = HexGrid(boardWidth, boardHeight);
    gameBoard.initializeDefaultBoard();

    std::vector<Missile> missiles;
    Missile testMissile = Missile(7, 3, Direction::SE);
    // raw update to game board now, need function to handle when multiple missiles are created
    gameBoard.GB[7][3].hasMissile = true;
    missiles.push_back(testMissile);

    // testing printing the game board
    gameBoard.printGrid();

    // GAME LOOP
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
                    clients.push_back({newClient, '\0'});
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
                    char tempBuffer[1024] = {0};
                    int bytes = read(client.fd, tempBuffer, sizeof(tempBuffer) - 1);
                    if(bytes <= 0){
                        std::cout << "[INFO] Client disconnected (read failure): " << client.fd << "\n";
                        close(client.fd);
                        pollFds.erase(pollFds.begin() + i);
                        clients.erase(clients.begin() + clientIndex);
                        continue;
                    }
                    if(bytes > 0){
                        tempBuffer[bytes] = '\0';
                        // std::cout << "[DEBUG] Received raw: '" << client.buffer.data() << "'\n";
                        try{
                            std::string jsonStr(tempBuffer, bytes);
                            auto j = json::parse(jsonStr);
                            auto it = j.find("input");
                            if(it != j.end() && it->is_string()){
                                std::string val = *it;
                                clients[clientIndex].last_input = val[0];
                                std::cout << "[INFO] Client " << client.fd << " Pressed " << val[0] << "\n";
                            } else{
                                std::cout << "[WARNING] Unkown message format.\n";
                            }
                        } catch(json::parse_error &e){
                            std::cout << "[DEBUG] In catch statement for parsing input\n";
                            std::cerr << "[ERROR] JSON parse error: " << e.what() << "\n";
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
            // UPDATE THE BOARD
            updateMissiles(missiles, gameBoard);
            gameBoard.printGrid();
            // construct json to send to server based on input
            json update_j;
            update_j["type"] = "update";
            update_j["clients"] = json::array();
            for(const auto& client : clients){
                json client_info;
                client_info["id"] = client.fd;
                client_info["last_input"] = std::string(1, client.last_input);
                update_j["clients"].push_back(client_info);
                // std::cout << "PUSHING INFO:\nCLIENT: " + std::to_string(client.fd) + "\nLAST_INPUT: " + client.last_input + "\n";
            }
            std::string sj = update_j.dump();
            for(auto& client : clients){
                send(client.fd, sj.c_str(), sj.size(), 0);
            }
            last_sent = now;
        }
    }
    close(serverSocket);
    return 0;
}










