/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Utility.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 12:56 PM
 */

#ifndef UTILITY_H
#define UTILITY_H
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <error.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include "SentLessThenExpectedException.h"
using namespace std;

class Utility
{
public:
    Utility();
    Utility(const Utility& orig);
    virtual ~Utility();
    static int IsRegularFile(string &path);
    static string GetFileNameFromPath(string &path);
    static string TimeToString(time_t &time);
    static time_t GetLastModificationDate(string &path);
    static string GetFirstElementOfPath(string &path);
    static ssize_t ReadDataFromSocket(int fd, char * buffer, ssize_t buffsize);
    static void WriteDataToScoket(int fd, char * buffer, ssize_t count);
    static int OpenListeningSocket(char *ip, char *port);
    static void CloseSocket(int sd);
    static void SetSocketToNonBlocking(int sd);
    static bool ValidateIP(char *ip);
    static bool ValidatePort(string port);
    static bool ValidatePath(string path);
private:
    static uint16_t readPort(char * txt);
    static void setReuseAddr(int sock);
};

#endif /* UTILITY_H */

