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
#include <fcntl.h>
#include <math.h>
#include <utime.h>

using namespace std;

typedef uint_fast32_t crc_t;

class Utility
{
public:
    Utility();
    Utility(const Utility& orig);
    virtual ~Utility();
    static int IsRegularFile(string &path);
    static string GetFileNameFromPath(string &path);
    static string TimeToString(time_t &time);
    static time_t GetLastModificationDate(string path);
    static void UpdateLastModificationDate(string path, time_t lastModificationDate);
    static string GetFirstElementOfPath(string &path);
    static crc_t CalculateCRC32OfFile(string path);
private:
    static inline crc_t crc_init(void);
    static inline crc_t crc_finalize(crc_t crc);
    static crc_t crc_update(crc_t crc, const char *data, size_t data_len);
};

#endif /* UTILITY_H */

