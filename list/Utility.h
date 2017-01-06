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
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <sstream>
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
private:

};

#endif /* UTILITY_H */

