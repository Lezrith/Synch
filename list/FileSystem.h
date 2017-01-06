/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FileSystem.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 5, 2017, 5:19 PM
 */

#ifndef FILESYSTEM_H
#define FILESYSTEM_H
#include <mutex>
#include <string>
#include "File.h"

class FileSystem
{
public:
    FileSystem();
    FileSystem(string &path);
    FileSystem(const FileSystem& orig);
    virtual ~FileSystem();

    string ToString();
    void FromString(string s);
    void SaveToFile(string &path);
    void LoadFromFile(string &path);
    deque<Event*> FindDifferences(FileSystem *fs);
    void InsertFile(File *f);
    void DeleteFileAt(string path);
    void ReplaceFile(File *f);
private:
    mutex mx;
    File *root = nullptr;

};

#endif /* FILESYSTEM_H */

