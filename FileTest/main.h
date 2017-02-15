/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   main.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 12:50 PM
 */

#ifndef MAIN_H
#define MAIN_H
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept>
#include <stdio.h>
#include <sys/file.h>
#include <deque>
#include <error.h>
#include <fcntl.h>
#include <thread>
#include <sys/poll.h>
#include <set>
#include <cstring>
#include <limits.h>
#include "FileSystem.h"
#include "Event.h"
#include "SentLessThenExpectedException.h"
using namespace std;

#define GetCurrentDir getcwd
void AcceptConnectionsInLoop(char *ip, int socket);
void AddDescriptorToPoll(int fd, short event);
void RemoveDescriptorFromPoll(int fd);
void AcceptConnectionFromClient();
void SendToAllBut(int fd, char * buffer, int count);
#endif /* MAIN_H */

