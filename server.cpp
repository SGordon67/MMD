// TODO: add a countdown to the missiles so they can detonate without hitting structure?

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
            return '.';
            break;
        case hexState::city:
            return '^';
            break;
        case hexState::player:
            return 'Y';
            break;
        case hexState::wall:
            return '|';
            break;
        case hexState::destroyed:
            return 'X';
            break;
        case hexState::blank:
            return '?';
            break;
        default:
            return '~';
    }
    return '~';
}

enum class Direction{
    SE = 0,
    NE = 1,
    N = 2,
    NW = 3,
    SW = 4,
    S = 5
};
struct coord{
    int q;
    int r;

};
const coord nDiff[2][6] = {
    // 0 = southeast
    // 1 = northeast
    // 2 = north
    // 3 = northwest
    // 4 = southwest
    // 5 = south

    // even columns
    {{1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1, 0}, {0, 1}},
    // odd columns
    {{1,1}, {1,0}, {0,-1}, {-1,0},{-1,1}, {0,1}}
};
coord getN(coord cd, Direction dir){
    int parity = cd.q & 1;

    coord delta = nDiff[parity][static_cast<int>(dir)];
    delta.q += cd.q;
    delta.r += cd.r;

    return delta;
}

struct Missile{
    coord cd;
    Direction dir;
    int width;
    int countDown;

    Missile(){
    }
    Missile(int q, int r, int cd, Direction dir){
        this->cd = {q, r};
        this->dir = dir;
        this->countDown = cd;
        this->width = 3;
    }
};

struct Hex{
    coord cd;
    bool hasMissile;
    hexState state;
    int stopVal; // travel time through hex

    bool operator==(const Hex& other) const{
        return (this->cd.q == other.cd.q && this->cd.r == other.cd.r);
    }
    Hex(){
        this->cd = {0, 0};
        this->hasMissile = false;
        this->state = hexState::blank;
        this->stopVal = 0;
    }
    Hex(int q, int r, int sv, bool hasMissile, hexState state){
        this->cd = {q, r};
        this->stopVal = sv;
        this->hasMissile = hasMissile;
        this->state = state;
    }
};

struct HexGrid{
    int boardWidth;
    int boardHeight;
    std::vector<std::vector<Hex>> GB;
    std::vector<Missile> missiles;

    HexGrid(int w, int h){
        this->boardWidth = w;
        this->boardHeight = h;
        this->GB = {};
        this->missiles = {};
    }
    HexGrid(int w, int h, std::vector<std::vector<Hex>> GB, std::vector<Missile> missiles){
        this->boardWidth = w;
        this->boardHeight = h;
        this->GB = GB;
        this->missiles = missiles;
    }
    void initializeDefaultBoard(){
        this->GB.resize(boardWidth, std::vector<Hex>(boardHeight));
        this->missiles = {};
        for(int y = 0; y < this->boardHeight; y++){
            for(int x = 0; x < this->boardWidth; x++){
                Hex newHex;
                if(x == 0 || x == this->boardWidth - 1){
                    if(y == 0){
                        newHex = Hex(x, y, 0, false, hexState::blank);
                    } else{
                        newHex = Hex(x, y, 0, false, hexState::wall);
                    }
                }else if(y == this->boardHeight - 1){
                    if(x == boardWidth/2){
                        newHex = Hex(x, y, 0, false, hexState::player);
                    }else{
                        newHex = Hex(x, y, 0, false, hexState::city);
                    }
                }else if(y == 0){
                    if(x%2){
                        newHex = Hex(x, y, 0, false, hexState::city);
                    }else{
                        newHex = Hex(x, y, 0, false, hexState::blank);
                    }
                }else if(y == 1){
                    if(x%2){
                        newHex = Hex(x, y, 0, false, hexState::air);
                    }else{
                        if(x == boardWidth/2){
                            newHex = Hex(x, y, 0, false, hexState::player);
                        }else{
                            newHex = Hex(x, y, 0, false, hexState::city);
                        }
                    }
                } else{
                    newHex = Hex(x, y, 0, false, hexState::air);
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
                if(this->GB[x][y].hasMissile) placeMarker = 'V';
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
    std::vector<coord> getRingCoords(int radius, coord cd){
        std::vector<coord> ringCoords;
        if(radius == 0){
            ringCoords.push_back(cd);
            return ringCoords;
        }
        // move out 'radius' units from the hex
        for(int i = 0; i < radius; i++){
            cd = getN(cd, Direction::SE);
        }
        Direction circ[] = {Direction::N, Direction::NW, Direction::SW, Direction::S, Direction::SE, Direction::NE};
        // circle the middle, saving each coord
        for(int i = 0; i < 6; i++){
            for(int j = 0; j < radius; j++){
                cd = getN(cd, circ[i]);
                ringCoords.push_back(cd);
            }
        }
        return ringCoords;
    }
    std::vector<coord> getCircleCoords(int radius, coord cd){
        std::vector<coord> circleCoords;
        for(int i = 0; i < radius; i++){
            std::vector<coord> tmpRing = getRingCoords(i, cd);
            circleCoords.insert(circleCoords.end(), tmpRing.begin(), tmpRing.end());
        }
        return circleCoords;
    }
    void destroyHex(coord cd){
        this->GB[cd.q][cd.r].state = hexState::destroyed;
    }
    void destroyArea(std::vector<coord> cds){
        for(int i = 0; i < cds.size(); i++){
            if(cds[i].q < 0 || cds[i].q > this->boardWidth - 1) continue;
            if(cds[i].r < 0 || cds[i].r > this->boardHeight - 1) continue;
            switch(this->GB[cds[i].q][cds[i].r].state){
                case hexState::city:
                case hexState::player:
                case hexState::wall:
                    destroyHex(cds[i]);
                    break;
                case hexState::air:
                    destroyHex(cds[i]);
                    break;
            }
        }
    }
    // gather all the coordinates relevant to the explosion, hand over to destroy function
    void detonate(int index){
        std::vector<coord> destroyCoords = getCircleCoords(this->missiles[index].width, this->missiles[index].cd);
        this->destroyArea(destroyCoords);
        return;
    }
    void updateMissiles(){
        for(int i = 0; i < this->missiles.size(); i++){
            int q = this->missiles[i].cd.q;
            int r = this->missiles[i].cd.r;

            if(this->missiles[i].countDown > 0){
                this->missiles[i].countDown--;
            } else{
                // get coords of new position
                // delta here means the new coordinates, not change in coords as you would expect
                coord delta = getN(this->missiles[i].cd, this->missiles[i].dir);

                // correct if this->missiles goes past boundaries
                if(delta.q >= this->boardWidth){
                    delta.q = this->boardWidth-1;
                } else if(delta.r >= this->boardHeight){
                    delta.r = this->boardHeight-1;
                }
                if(delta.q <= 0){
                    delta.q = 0;
                } else if(delta.r <= 0){
                    delta.r = 0;
                }

                // update the missiles position first
                this->missiles[i].cd.q = delta.q;
                this->missiles[i].cd.r = delta.r;
                // update the countdown to reflect new hex
                // countdown, again, meaning the time to move to next hex, not an explosion countdown
                this->missiles[i].countDown = this->GB[delta.q][delta.r].stopVal;
                this->GB[q][r].hasMissile = false;

                std::cout << "[DEBUG] New missiles position: " << this->missiles[i].cd.q << ", " << this->missiles[i].cd.r << "\n";

                // detect collision and remove this->missiles
                switch(this->GB[delta.q][delta.r].state){
                    case hexState::city:
                    case hexState::player:
                    case hexState::wall:
                        this->detonate(i);
                        // std::cout << "[DEBUG] made it through detonate function\n";
                        this->missiles.erase(this->missiles.begin() + i);
                        this->GB[delta.q][delta.r].hasMissile = false;
                        i--; // adjust index to reflect missing missile
                        break;
                    case hexState::air:
                        this->GB[delta.q][delta.r].hasMissile = true;
                        break;
                }
                // std::cout << "Moving from (" << r << ", " << q << ") to (" << delta.r << ", " << delta.q << ")\n";
            }
            return;
        }
    }
};

int main(){
    // delay for game updates
    auto updateDelay = std::chrono::milliseconds(500);
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

    // raw update to game board now, need function to handle when multiple missiles are created
    int mq = 4;
    int mr = 5;
    Missile testMissile = Missile(mq, mr, gameBoard.GB[mq][mr].stopVal, Direction::SE);
    gameBoard.GB[mq][mr].hasMissile = true;
    gameBoard.missiles.push_back(testMissile);

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
            gameBoard.updateMissiles();
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










