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
}

FileSystem::FileSystem(string& path) : path(path)
{
    mx.lock();
    string rootName = this->path.GetFilename();
    this->root = new File(rootName);
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
    string result = this->path.ToString();
    result += "\n" + root->ToString();
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
        Path p(path);
        root->DeleteFileAt(p);
    } catch (FileDoesNotExistException ex)
    {
        cout << ex.what() << endl;
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
        cout << ex.what() << endl;
        //TODO: handle this exception
    } catch (FileIsNotDirectoryException ex)
    {
        cout << ex.what() << endl;
        //TODO: handle this exception
    } catch (FileOutsideRootDirectoryException ex)
    {
        cout << ex.what() << endl;
        //TODO: handle this exception
    }
    mx.unlock();
}

void FileSystem::ReplaceFile(File* f)
{
    mx.lock();
    try
    {
        root->ReplaceFile(f);
    } catch (FileDoesNotExistException ex)
    {
        cout << ex.what() << endl;
        //TODO: handle this exception
    } catch (ReplacingNewerFileException ex)
    {
        cout << ex.what() << endl;
        //TODO: handle this exception
    }
    mx.unlock();
}

Path FileSystem::MakePathAbsolute(Path path)
{
    Path p(this->path.ToString() + "/" + path.ToString());
    return p;
}

Path FileSystem::MakePathRelative(Path path)
{
    Path p(path.ToString().replace(0, this->path.ToString().length() + 1, ""));
}
