all: client.o server.o
	g++ client.cpp -lncurses -std=c++11 -o client
	g++ server.cpp -lncurses -std=c++11 -o server
client.o: client.cpp
	g++ -std=c++11 -c client.cpp
server.o: server.cpp
	g++ -std=c++11 -c server.cpp

clean:
	rm *.o
	rm client
	rm server
