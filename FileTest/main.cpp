/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   main.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on January 2, 2017, 11:20 AM
 */

#include "main.h"
/*
 *
 */
const int MAXDESCRIPTORS = 10;
const ssize_t BUFFERSIZE = 512;
string path = "./test";
FileSystem *fs;
pollfd descriptors[MAXDESCRIPTORS];
mutex clientsMutex;
int numberOfActiveDescriptors = 0;
bool exitLoop = false;
int listeningSocket;

int main(int argc, char** argv)
{
    if (argc != 3) error(1, 0, "Need 2 args");
    char *ip = argv[1];
    char *port = argv[2];
    char buffer[BUFFERSIZE];

    cout << ip << " " << port << endl;

    if (!Utility::ValidateIP(ip))
    {
        cout << "Provided IP is not valid." << endl;
        exit(1);
    }

    /*listeningSocket = Utility::OpenListeningSocket(ip, port);
    Utility::SetSocketToNonBlocking(listeningSocket);

    AddDescriptorToPoll(listeningSocket, POLLIN);

    int exitCode;
    ssize_t bytesRecived;
    do
    {
        printf("Waiting on poll()...\n");
        exitCode = poll(descriptors, numberOfActiveDescriptors, -1);
        if (exitCode > 0)
        {
            for (int i = 1; i < numberOfActiveDescriptors; i++)
            {
                if (descriptors[i].revents == POLLIN)
                {
                    bytesRecived = Utility::ReadDataFromSocket(descriptors[i].fd, buffer, BUFFERSIZE);
                    if (bytesRecived == 0)
                    {
                        RemoveDescriptorFromPoll(descriptors[i].fd);
                        i--;
                    }
                    else
                    {
                        SendToAllBut(descriptors[i].fd, buffer, bytesRecived);
                        if (strcmp(buffer, "END\n") == 0) exitLoop = true;
                    }
                }
            }
            if (descriptors[0].revents == POLLIN)
            {
                AcceptConnectionFromClient();
            }
        }
        else
        {
            perror("poll() failed\n");
            break;
        }

    } while (!exitLoop);

    for (int i = 0; i < numberOfActiveDescriptors; i++)
    {
        if (descriptors[i].fd >= 0)Utility::CloseSocket(descriptors[i].fd);
    }*/
    char *realPath;
    realpath(path.c_str(), realPath);
    string p = realPath;
    cout << realPath << endl;
    FileSystem *fs = new FileSystem(p);
    FileSystem *fs2 = new FileSystem();
    File *f = new File("test/aaa/bbb/a", 0, nullptr);
    string save = "./save";
    fs2->LoadFromFile(save);
    fs->InsertFile(f);
    fs->DeleteFileAt("test/c");
    auto changes = fs->FindDifferences(fs2);
    for (auto elem : changes)
    {
        cout << elem->ToString() << endl;
    }
    cout << fs->ToString();
    cout << fs2->ToString();
    return 0;
}

void AddDescriptorToPoll(int fd, short event)
{

    descriptors[numberOfActiveDescriptors].fd = fd;
    descriptors[numberOfActiveDescriptors].events = event;
    numberOfActiveDescriptors++;
}

void RemoveDescriptorFromPoll(int fd)
{
    for (int i = 0; i < numberOfActiveDescriptors; i++)
    {
        if (descriptors[i].fd == fd)
        {
            for (int j = i; j < numberOfActiveDescriptors && j < MAXDESCRIPTORS - 1; j++)
            {
                descriptors[j].fd = descriptors[j + 1].fd;
            }
            numberOfActiveDescriptors--;
            Utility::CloseSocket(fd);

            break;
        }
    }
    printf("Client using descriptor %d disconnected.\n", fd);
}

void AcceptConnectionFromClient()
{
    int clientSocket;
    sockaddr_in clientAddr{0};
    socklen_t clientAddrSize = sizeof (clientAddr);
    do
    {
        clientSocket = accept(listeningSocket, (sockaddr*) & clientAddr, &clientAddrSize);
        if (clientSocket < 0)
        {
            if (errno != EWOULDBLOCK)
            {
                perror("Accept() failed");
                exitLoop = true;
            }
            break;
        }
        printf("New incoming connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientSocket);
        AddDescriptorToPoll(clientSocket, POLLIN);
    } while (clientSocket != -1);
}

void SendToAllBut(int fd, char * buffer, int count)
{
    int res;
    vector<int> socketsToClose;
    for (int i = 1; i < numberOfActiveDescriptors; i++)
    {
        if (descriptors[i].fd != fd)
        {
            try
            {
                Utility::WriteDataToScoket(descriptors[i].fd, buffer, count);
            } catch (SentLessThenExcpectedException ex)
            {
                socketsToClose.push_back(descriptors[i].fd);
            }
        }
    }
    for (auto s : socketsToClose)
    {
        RemoveDescriptorFromPoll(s);
    }

}