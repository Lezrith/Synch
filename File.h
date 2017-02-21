/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   File.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 12:24 PM
 */

#ifndef FILE_H
#define FILE_H

#include <ctime>
#include <string>
#include <vector>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <fstream>
#include <deque>
#include "Path.h"
#include "Event.h"
#include "Utility.h"
using namespace std;

class File
{
public:
    Path path;
    File();
    File(string &path, string absolutePath);
    File(string path, time_t lastModificationDate, bool isDirectory);
    File(const File& orig);
    virtual ~File();

    string ToString();
    string ToStringShort();
    void FromString(string s);
    void SaveToFile(string &path);
    void LoadFromFile(string &path);
    deque<Event*> FindDifferences(File *f);
    File* GetFileFromPath(Path path);

    time_t GetLastModificationDate() const
    {
        return lastModificationDate;
    }
private:
    bool isDirectory;
    File* parent = nullptr;
    string name;
    time_t lastModificationDate = 0;
    vector<File*> children;

    deque<Event*> FindDeletes(File *f);
    File* GetChildByName(string &name);
};

#endif /* FILE_H */

