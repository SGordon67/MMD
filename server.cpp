// TODO: solve top of board collision problem, maybe padding layer?

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
            return ' ';
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
        this->width = 1;
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
        return ringCoords;
    }
    void destroyHex(coord cd){
        this->GB[cd.q][cd.r].state = hexState::destroyed;
    }
    void destroyRadius(int radius, coord cd){
        std::vector<Hex> ring;
        coord it = {cd.q, cd.r};
        return;
    }
};

void explode(Missile& missile, HexGrid& grid){
    // get all six neighbor coords
    int parity = missile.cd.q % 2;
    coord deltaSE = nDiff[parity][static_cast<int>(Direction::SE)];
    coord deltaNE = nDiff[parity][static_cast<int>(Direction::NE)];
    coord deltaN = nDiff[parity][static_cast<int>(Direction::N)];
    coord deltaNW = nDiff[parity][static_cast<int>(Direction::NW)];
    coord deltaSW = nDiff[parity][static_cast<int>(Direction::SW)];
    coord deltaS = nDiff[parity][static_cast<int>(Direction::S)];

    if(missile.cd.r == grid.boardHeight-1){ // bottom
        std::cout << "[DEBUG] Bottom collision detected\n";
        grid.destroyHex(missile.cd);
        // grid.GB[missile.cd.q][missile.cd.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaN.q][missile.cd.r+deltaN.r].state = hexState::destroyed;
        if(missile.cd.q == grid.boardWidth-1){ // bottom right
            grid.GB[missile.cd.q+deltaSW.q][missile.cd.r+deltaSW.r].state = hexState::destroyed;
            grid.GB[missile.cd.q+deltaNW.q][missile.cd.r+deltaNW.r].state = hexState::destroyed;
        }else if(missile.cd.q == 0){ // bottom left
            grid.GB[missile.cd.q+deltaSE.q][missile.cd.r+deltaSE.r].state = hexState::destroyed;
            grid.GB[missile.cd.q+deltaNE.q][missile.cd.r+deltaNE.r].state = hexState::destroyed;
        }else{ // bottom only
            grid.GB[missile.cd.q+deltaNE.q][missile.cd.r+deltaNE.r].state = hexState::destroyed;
            grid.GB[missile.cd.q+deltaNW.q][missile.cd.r+deltaNW.r].state = hexState::destroyed;
            if(!(missile.cd.q%2)){ // even column only
                grid.GB[missile.cd.q+deltaSW.q][missile.cd.r+deltaSW.r].state = hexState::destroyed;
                grid.GB[missile.cd.q+deltaSE.q][missile.cd.r+deltaSE.r].state = hexState::destroyed;
            }
        }
    }else if(missile.cd.r == 0){ // top collision
        std::cout << "[DEBUG] Top collision detected\n";
        grid.GB[missile.cd.q][missile.cd.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaS.q][missile.cd.r+deltaS.r].state = hexState::destroyed;
        if(missile.cd.q == grid.boardWidth-1){ // top right
            grid.GB[missile.cd.q+deltaSW.q][missile.cd.r+deltaSW.r].state = hexState::destroyed;
        }else if(missile.cd.q == 0){ // top left
            grid.GB[missile.cd.q+deltaSE.q][missile.cd.r+deltaSE.r].state = hexState::destroyed;
        }else{ // top
            grid.GB[missile.cd.q+deltaSE.q][missile.cd.r+deltaSE.r].state = hexState::destroyed;
            grid.GB[missile.cd.q+deltaSW.q][missile.cd.r+deltaSW.r].state = hexState::destroyed;
            if(missile.cd.q%2){ // odd column only
                grid.GB[missile.cd.q+deltaNW.q][missile.cd.r+deltaNW.r].state = hexState::destroyed;
                grid.GB[missile.cd.q+deltaNE.q][missile.cd.r+deltaNE.r].state = hexState::destroyed;
            }
        }
    }else if(missile.cd.q == grid.boardWidth-1){ // right collision
        std::cout << "[DEBUG] Right collision detected\n";
        grid.GB[missile.cd.q+deltaS.q][missile.cd.r+deltaS.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaSW.q][missile.cd.r+deltaSW.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaNW.q][missile.cd.r+deltaNW.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaN.q][missile.cd.r+deltaN.r].state = hexState::destroyed;
    }else if(missile.cd.q == 0){ // left collision
        std::cout << "[DEBUG] Left collision detected\n";
        grid.GB[missile.cd.q+deltaS.q][missile.cd.r+deltaS.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaSE.q][missile.cd.r+deltaSE.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaNE.q][missile.cd.r+deltaNE.r].state = hexState::destroyed;
        grid.GB[missile.cd.q+deltaN.q][missile.cd.r+deltaN.r].state = hexState::destroyed;
    }
}

void updateMissiles(std::vector<Missile>& missiles, HexGrid& grid){
    for(int i = 0; i < missiles.size(); i++){
        int q = missiles[i].cd.q;
        int r = missiles[i].cd.r;

        if(missiles[i].countDown > 0){
            missiles[i].countDown--;
        } else{
            // get coords of new position
            coord delta = getN(missiles[i].cd, missiles[i].dir);

            // correct if missile goes past boundaries
            if(delta.q >= grid.boardWidth){
                delta.q = grid.boardWidth-1;
            } else if(delta.r >= grid.boardHeight){
                delta.r = grid.boardHeight-1;
            }
            if(delta.q <= 0){
                delta.q = 0;
            } else if(delta.r <= 0){
                delta.r = 0;
            }

            // update the missile position first
            missiles[i].cd.q = delta.q;
            missiles[i].cd.r = delta.r;
            // update the countdown to reflect new hex
            missiles[i].countDown = grid.GB[delta.q][delta.r].stopVal;
            grid.GB[q][r].hasMissile = false;

            std::cout << "[DEBUG] New Missile position: " << missiles[i].cd.q << ", " << missiles[i].cd.r << "\n";

            // detect collision and remove missile
            switch(grid.GB[delta.q][delta.r].state){
                case hexState::city:
                case hexState::player:
                case hexState::wall:
                    explode(missiles[i], grid);
                    // std::cout << "[DEBUG] made it through explode function\n";
                    missiles.erase(missiles.begin() + i);
                    grid.GB[delta.q][delta.r].hasMissile = false;
                    i--;
                    break;
                case hexState::air:
                    grid.GB[delta.q][delta.r].hasMissile = true;
                    break;
            }
            // std::cout << "Moving from (" << r << ", " << q << ") to (" << delta.r << ", " << delta.q << ")\n";
        }
        return;
    }
}

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
    std::vector<Missile> missiles;
    Missile testMissile = Missile(mq, mr, gameBoard.GB[mq][mr].stopVal, Direction::SE);
    gameBoard.GB[mq][mr].hasMissile = true;
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










