/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FileSystem.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on January 5, 2017, 5:19 PM
 */

#include "FileSystem.h"

FileSystem::FileSystem() : path(string(""))
{
    this->root = new File;
}

FileSystem::FileSystem(string& path) : path(path)
{
    mx.lock();
    string rootName = "";
    this->root = new File(rootName, path);
    mx.unlock();
}

FileSystem::FileSystem(const FileSystem& orig) : path(orig.path)
{
}

FileSystem::~FileSystem()
{
    delete root;
}

string FileSystem::ToString()
{
    mx.lock();
    //string result = this->path.ToString();
    string result = root->ToString();
    mx.unlock();
    return result;
}

void FileSystem::SaveToFile(string& path)
{
    mx.lock();
    root->SaveToFile(path);
    mx.unlock();
}

void FileSystem::LoadFromFile(string& path)
{
    mx.lock();
    root = new File();
    root->LoadFromFile(path);
    mx.unlock();
}

void FileSystem::FromString(string s)
{
    mx.lock();
    root->FromString(s);
    mx.unlock();
}

deque<Event*> FileSystem::FindDifferences(FileSystem *f)
{
    mx.lock();
    deque<Event*> result = root->FindDifferences(f->root);
    mx.unlock();
    return result;
}

Path FileSystem::MakePathAbsolute(Path path)
{
    Path p(this->path.GetPath() + "/" + path.GetPath());
    return p;
}

Path FileSystem::MakePathRelative(Path path)
{
    Path p(path.GetPath().replace(0, this->path.GetPath().length() + 1, ""));
    return p;
}

File* FileSystem::GetFileByPath(string path)
{
    mx.lock();
    return root->GetFileFromPath(path);
    mx.unlock();
}
