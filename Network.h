#ifndef NETWORK_H
#define NETWORK_H

#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <ftw.h>
#include <string.h>
#include <stdexcept>
#include "Utility.h"
#define BUFLEN 65536
#define MAX_RECONN_ATTEMPTS 5
#define SMALLBUF 32

using namespace std;

int receive_file(int, char*, char*, bool, set<string> *);
int receive_message(int, char*);
int send_ch_arr(int, const char*, string, int);
void send_string(int, string, string);
int send_file(int, const char*, char*);
void sendLastModificationDate(int clientSocket, string absolutePath);
time_t receiveModificationDate(int clientSocket);
void send_big_string(int, string, string);
string receive_big_string(int);

#endif
