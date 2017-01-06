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

FileSystem::FileSystem()
{
}

FileSystem::FileSystem(string& path)
{
    mx.lock();
    this->root = new File(path);
    mx.unlock();
}

FileSystem::FileSystem(const FileSystem& orig)
{
}

FileSystem::~FileSystem()
{
    delete root;
}

string FileSystem::ToString()
{
    mx.lock();
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

void FileSystem::DeleteFileAt(string path)
{
    mx.lock();
    try
    {
        root->DeleteFileAt(path);
    } catch (FileDoesNotExistException ex)
    {
        //TODO: handle this exception
    }

    mx.unlock();
}

void FileSystem::InsertFile(File* f)
{
    mx.lock();
    try
    {
        root->InsertFileAt(f, f->path);
    } catch (FileAlreadyExistsException ex)
    {
        //TODO: handle this exception
    } catch (FileIsNotDirectoryException ex)
    {
        //TODO: handle this exception
    } catch (FileOutsideRootDirectoryException ex)
    {
        //TODO: handle this exception
    }
    mx.unlock();
}

void FileSystem::ReplaceFile(File* f)
{

}
