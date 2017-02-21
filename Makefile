all: Client Server
Client: Client.cpp
	g++ -std=c++11 -Wall -pthread -lm Client.cpp Network.cpp Event.cpp File.cpp FileSystem.cpp Path.cpp Utility.cpp -o Client
Server: Server.cpp
	g++ -std=c++11 -Wall -pthread -lm Server.cpp Network.cpp Event.cpp File.cpp FileSystem.cpp Path.cpp Utility.cpp -o Server
clean:
	rm Client Server
