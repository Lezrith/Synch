/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Utility.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 12:56 PM
 */

#include "Utility.h"

Utility::Utility()
{
}

Utility::Utility(const Utility& orig)
{
}

Utility::~Utility()
{
}

int Utility::IsRegularFile(string &path)
{
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) == -1)
    {
        cout << "Error(" << errno << ") opening " << path << endl;
        exit(EXIT_FAILURE);
    }
    return S_ISREG(path_stat.st_mode);
}

string Utility::GetFileNameFromPath(string& path)
{
    int lastSlashPosition = path.rfind("/");
    if (lastSlashPosition == -1) return path;
    else return path.substr(lastSlashPosition + 1, path.size() - 1);
}

string Utility::TimeToString(time_t& time)
{
    stringstream ss;
    ss << time;
    return ss.str();
}

time_t Utility::GetLastModificationDate(string& path)
{
    struct stat attrib;
    if (stat(path.c_str(), &attrib) == -1)
    {
        cout << "Error(" << errno << ") opening " << path << endl;
        exit(EXIT_FAILURE);
    }
    return attrib.st_mtime;
}

string Utility::GetFirstElementOfPath(string& path)
{
    int startPos = 0;
    if (path[0] == '.' && path[1] == '/') startPos = 2;
    if (path[0] == '/') startPos = 1;

    int firstSlash = path.substr(startPos).find("/");
    if (firstSlash != string::npos) return path.substr(startPos, firstSlash);
    else return path.substr(startPos);
}
