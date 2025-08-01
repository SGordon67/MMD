// TODO: Have server send the 'game state' to the client so that clients can view/reasonably play the game
// once more functionality is added i.e. firing rockets, creating barrier, etc...

// TODO: Clean up the client/server connect/disconnect functionality

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

int playerCount = 0;
bool anyClientEverConnected = false;
auto updateDelay = std::chrono::milliseconds(1000);
auto last_sent = std::chrono::steady_clock::now();


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
};

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

    // Member function equality operator
    bool operator==(const coord& other) const{
        return (q == other.q && r == other.r);
    }
    bool operator!=(const coord& other) const{
        return !(q == other.q && r == other.r);
    }
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
};

struct Client{
    int fd;
    int playerNum;
    coord cd;

    // only for testing
    char last_input;
};

struct Missile{
    coord cd;
    Direction dir;
    int radius;
    int speed;
    int timeLeftOnHex;

    Missile(){
    }
    Missile(int q, int r, Direction dir, int radius, int speed, int timeLeftOnHex){
        this->cd = {q, r};
        this->dir = dir;
        this->radius = radius;
        this->speed = speed;
        this->timeLeftOnHex = timeLeftOnHex;
    }
};

struct Hex{
    coord cd;
    hexState state;
    int density; // time to travel through hex

    bool hasMissile;
    int missileIndex;

    bool operator==(const Hex& other) const{
        return (this->cd.q == other.cd.q && this->cd.r == other.cd.r);
    }
    Hex(){
        this->cd = {0, 0};
        this->hasMissile = false;
        this->missileIndex = -1;
        this->state = hexState::blank;
        this->density = 1;
    }
    Hex(int q, int r, int density, bool hasMissile, hexState state){
        this->cd = {q, r};
        this->density = density;
        this->hasMissile = hasMissile;
        this->state = state;
    }
};

struct HexGrid{
    int boardWidth;
    int boardHeight;
    std::vector<std::vector<Hex>> GB;
    std::vector<Missile> missiles;
    std::vector<Client> clients;

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
                        newHex = Hex(x, y, 10, false, hexState::blank);
                    } else{
                        newHex = Hex(x, y, 10, false, hexState::wall);
                    }
                }else if(y == this->boardHeight - 1){
                    if(x == boardWidth/2){
                        newHex = Hex(x, y, 10, false, hexState::player);
                    }else{
                        newHex = Hex(x, y, 10, false, hexState::city);
                    }
                }else if(y == 0){
                    if(x%2){
                        newHex = Hex(x, y, 10, false, hexState::city);
                    }else{
                        newHex = Hex(x, y, 10, false, hexState::blank);
                    }
                }else if(y == 1){
                    if(x%2){
                        newHex = Hex(x, y, 1, false, hexState::air);
                    }else{
                        if(x == boardWidth/2){
                            newHex = Hex(x, y, 10, false, hexState::player);
                        }else{
                            newHex = Hex(x, y, 10, false, hexState::city);
                        }
                    }
                } else{
                    newHex = Hex(x, y, 1, false, hexState::air);
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
        for(int i = 0; i <= radius; i++){
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
        std::vector<coord> destroyCoords = getCircleCoords(this->missiles[index].radius, this->missiles[index].cd);
        this->destroyArea(destroyCoords);
        return;
    }
    void updateMissiles(){
        for(int i = 0; i < this->missiles.size(); i++){
            int moves = this->missiles[i].speed;
            coord curPOS = this->missiles[i].cd;
            coord newPOS = this->missiles[i].cd;
            while(moves > 0){
                // std::cout << "[DEBUG] Current moves left for missile [" << i << "]: " << moves << "\n";
                if(moves >= this->missiles[i].timeLeftOnHex){
                    moves -= this->missiles[i].timeLeftOnHex;
                    newPOS = getN(this->missiles[i].cd, this->missiles[i].dir);
                    // correct if this->missiles goes past boundaries
                    if(newPOS.q > this->boardWidth || newPOS.q < 0 || newPOS.r > this->boardHeight || newPOS.r < 0){
                        // std::cout << "[DEBUG] Missile attempted to go out of bounds, rubberbanded back to board\n";
                        newPOS = curPOS;
                    }
                    // std::cout << "[DEBUG] curPOS: [" << curPOS.q << "," << curPOS.r << "], hasMissile: " << this->GB[curPOS.q][curPOS.r].hasMissile << "\n";
                    // update the missiles position
                    this->missiles[i].cd.q = newPOS.q;
                    this->missiles[i].cd.r = newPOS.r;
                    this->missiles[i].timeLeftOnHex = this->GB[newPOS.q][newPOS.r].density;
                    this->GB[curPOS.q][curPOS.r].hasMissile = false;
                    this->GB[newPOS.q][newPOS.r].hasMissile = true;
                    this->GB[curPOS.q][curPOS.r].missileIndex = -1;
                    this->GB[newPOS.q][newPOS.r].missileIndex = i;
                    // detect collision and remove this->missiles
                    switch(this->GB[newPOS.q][newPOS.r].state){
                        case hexState::city:
                        case hexState::player:
                        case hexState::wall:
                            this->detonate(i);
                            this->missiles.erase(this->missiles.begin() + i);
                            this->GB[newPOS.q][newPOS.r].hasMissile = false;
                            this->GB[newPOS.q][newPOS.r].missileIndex = -1;
                            moves = 0;
                            i--; // adjust index to reflect missing missile
                            break; 
                        case hexState::air: 
                            break; 
                        default: 
                            break;
                    }
                    curPOS = newPOS;
                } else{
                    this->missiles[i].timeLeftOnHex -= moves;
                    moves = 0;
                }
                // std::cout << "[DEBUG] New missiles position: " << this->missiles[i].cd.q << ", " << this->missiles[i].cd.r << "\n";
            }
        }
    }
    void heavyUpdate(){
        if(anyClientEverConnected && clients.empty()){
            std::cout << "[INFO] All clients disconnected.\n";
            // break;
        }
        this->updateMissiles();
        this->printGrid();
        json update_j;
        update_j["type"] = "gameplayUpdate";
        update_j["clients"] = json::array();
        for(const auto& client : clients){
            json client_info;
            client_info["id"] = client.fd;
            client_info["last_input"] = std::string(1, client.last_input);
            update_j["clients"].push_back(client_info);
        }
        std::string sj = update_j.dump();
        for(auto& client : clients){
            send(client.fd, sj.c_str(), sj.size(), 0);
        }
    }
    void lightUpdate(){
    }
};

int setUpSocketsForLoop(int& serverSocket, std::vector<pollfd>& pollFds, std::vector<Client>& clients){
    pollFds.push_back({serverSocket, POLLIN, 0});
    for(const auto& client : clients){
        pollFds.push_back({client.fd, POLLIN, 0});
    }

    return(poll(pollFds.data(), pollFds.size(), 100));
};

int setUpSockets(int& serverSocket, sockaddr_in& serverAddress, std::vector<Client>& clients){
    // create server socket
    if(serverSocket < 0){
        perror("socket creation failed");
        return 1;
    }
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // specifying address
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

    while(playerCount < 1){
        std::vector<pollfd> pollFds;
        int ret = setUpSocketsForLoop(serverSocket, pollFds, clients);
        if(ret > 0){
            if(pollFds[0].revents & POLLIN){
                int newClient = accept(serverSocket, nullptr, nullptr);
                if(newClient >= 0){
                    pollFds.push_back({newClient, POLLIN, 0});
                    clients.push_back({newClient, '\0', playerCount});
                    anyClientEverConnected = true;

                    json j;
                    j["type"] = "pnUpdate";
                    j["playerNum"] = playerCount;
                    std::string message = j.dump();
                    send(newClient, message.c_str(), message.size(), 0);

                    playerCount++;
                }
            }
        }
    }
    std::cout << "[DEBUG] Two clients added, moving on to game loop\n";
    std::cout << "[DEBUG] Number of clients: " << clients.size() << std::endl;

    return 0;
};

int main(){
    const int boardWidth = 17;
    const int boardHeight = 15;
    HexGrid gameBoard = HexGrid(boardWidth, boardHeight);
    gameBoard.initializeDefaultBoard();

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddress;
    int ss = setUpSockets(serverSocket, serverAddress, gameBoard.clients);
    if(ss) return 1;

    // GAME LOOP
    while(true){
        std::vector<pollfd> pollFds;
        int ret = setUpSocketsForLoop(serverSocket, pollFds, gameBoard.clients);
        if(ret > 0){
            // check for a new connection
            if(pollFds[0].revents & POLLIN){
                int newClient = accept(serverSocket, nullptr, nullptr);
                if(newClient >= 0){
                    pollFds.push_back({newClient, POLLIN, 0});
                    gameBoard.clients.push_back({newClient, '\0'});
                    anyClientEverConnected = true;
                    playerCount++;
                }
            }
            for(int i = pollFds.size() - 1; i >= 1; i--){
                // detect hangup or disconnect error
                if(pollFds[i].revents & (POLLHUP | POLLERR)){
                    std::cout << "[INFO] Client disconnected (poll flags): " << pollFds[i].fd << "\n";
                    close(pollFds[i].fd);
                    pollFds.erase(pollFds.begin() + i);
                    gameBoard.clients.erase(gameBoard.clients.begin() + (i-1));
                    continue;
                }
                // check for readable input
                if(pollFds[i].revents & POLLIN){
                    // Get input from client
                    int clientIndex = i-1;
                    Client client = gameBoard.clients[clientIndex];
                    char tempBuffer[1024] = {0};
                    int bytes = read(client.fd, tempBuffer, sizeof(tempBuffer) - 1);
                    if(bytes <= 0){
                        std::cout << "[INFO] Client disconnected (read failure): " << client.fd << "\n";
                        close(client.fd);
                        pollFds.erase(pollFds.begin() + i);
                        gameBoard.clients.erase(gameBoard.clients.begin() + clientIndex);
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
                                gameBoard.clients[clientIndex].last_input = val[0];
                                std::cout << "[INFO] Client " << client.fd << " Pressed " << val[0] << "\n";
                            } else{
                                std::cout << "[WARNING] Unkown message format.\n";
                            }
                        } catch(json::parse_error &e){
                            std::cout << "[DEBUG] In catch statement for parsing input\n";
                            std::cerr << "[ERROR] JSON parse error: " << e.what() << "\n";
                        }
                    }
                }
            }
        }
        auto now = std::chrono::steady_clock::now();
        if(now - last_sent >= updateDelay){
            gameBoard.heavyUpdate();
            last_sent = now;
        } else{
            gameBoard.lightUpdate();
        }
    }
    close(serverSocket);
    return 0;
}










