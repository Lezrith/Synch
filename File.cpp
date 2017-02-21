/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   File.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 12:24 PM
 */

#include "File.h"

File::File() : path("")
{
    this->lastModificationDate = 0;
    this->parent = nullptr;
}

File::File(string &path, string absolutePath) : path(path)
{
    this->lastModificationDate = Utility::GetLastModificationDate(absolutePath);
    parent = nullptr;
    name = this->path.GetFilename();
    if (Utility::IsRegularFile(absolutePath))
    {
        isDirectory = false;
    }
    else
    {
        isDirectory = true;
        DIR * dir;
        dirent *dirEntries;
        if ((dir = opendir(absolutePath.c_str())) == NULL)
        {
            cout << "Error(" << errno << ") opening " << path << endl;
            exit(EXIT_FAILURE);
        }
        while ((dirEntries = readdir(dir)) != NULL)
        {
            string childName = string(dirEntries->d_name);
            if (childName != "." && childName != "..")
            {
                string childPath = path + "/" + childName;
                string absoluteChildPath = absolutePath + "/" + childName;
                File* child = new File(childPath, absoluteChildPath);
                child->parent = this;
                children.push_back(child);
            }
        }
        closedir(dir);
    }
}

File::File(string path, time_t lastModificationDate, bool isDirectory) : path(path)
{
    this->name = this->path.GetFilename();
    this->lastModificationDate = lastModificationDate;
    this->isDirectory = isDirectory;
}

File::File(const File& orig) : path(orig.path)
{
}

File::~File()
{
    for (auto c : children)
    {
        delete c;
    }
}

string File::ToString()
{
    string result = this->ToStringShort();
    result += "\n";
    for (auto c : children)
    {
        stringstream ss(c->ToString());
        string to;
        while (getline(ss, to, '\n'))
        {
            result += " " + to + "\n";
        }
    }
    return result;
}

string File::ToStringShort()
{
    string result = "";
    result += path.ToString() + " " + Utility::TimeToString(lastModificationDate);
    if (isDirectory) result += " d";
    else result += " n";
    return result;
}

void File::FromString(string s)
{
    if (this->path.ToString() != "0 ") throw invalid_argument("You tried to load into nonempty node.");
    stringstream ss(s);
    string to;
    getline(ss, to, '\n');
    to = path.FromString(to);
    lastModificationDate = stoi(to.substr(0, to.size() - 2), nullptr);
    name = path.GetFilename();
    if (to[to.size() - 1] == 'd') isDirectory = true;
    else isDirectory = false;

    string childString = "";
    /*while (getline(ss, to, '\n'))
    {
        if (to[0] == ' ')childString = to.substr(1, to.size() - 1) + "\n";
        else childString = to;
        while (getline(ss, to, '\n'))
        {
            if (to[0] == ' ' && to[1] == ' ')
            {
                childString += to.substr(1, to.size() - 1) + "\n";
            }
            else
            {
                File* newChild = new File();
                newChild->FromString(childString);
                children.push_back(newChild);
                if (to[0] == ' ')childString = to.substr(1, to.size() - 1) + "\n";
                else childString = to;
                break;
            }
        }
    }*/
    if (ss.rdbuf()->in_avail() != 0)
    {
        getline(ss, to, '\n');
        childString = to.substr(1, to.size() - 1) + "\n";
        do
        {
            while (getline(ss, to, '\n'))
            {
                if (to[0] == ' ' && to[1] == ' ')
                {
                    childString += to.substr(1, to.size() - 1) + "\n";
                }
                else
                {
                    File* newChild = new File();
                    newChild->FromString(childString);
                    children.push_back(newChild);
                    childString = to.substr(1, to.size() - 1) + "\n";
                    break;
                }
            }
        } while (ss.rdbuf()->in_avail() != 0);
    }
    if (childString != "")
    {
        File* newChild = new File();
        newChild->FromString(childString);
        children.push_back(newChild);
    }
}

File* File::GetFileFromPath(Path path)
{
    path.TakeFirstElement();
    if (path.GetPath() == this->name) return this;
    for (auto c : children)
    {
        if (path.GetPath() == c->name) return c;
        if (path.GetFirstElement() == c->name)
        {
            return c->GetFileFromPath(path);
        }
    }
    return nullptr;
}

void File::LoadFromFile(string& path)
{
    ifstream f;
    f.open(path);
    string contents((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    this->FromString(contents);
}

void File::SaveToFile(string& path)
{
    ofstream f;
    f.open(path);
    f << this->ToString();
    f.close();
}

//returns queue of events to perform on 'fs' in order to make it the same as this FileSystem

deque<Event*> File::FindDifferences(File* fs)
{
    deque<Event*> result, childEvents;
    string &&p = path.GetPath();
    File *other = fs->GetFileFromPath(p);
    if (other == nullptr) result.push_back(new Event(this->path, this->lastModificationDate, this->isDirectory, CREATE));
    else
    {
        if (!this->isDirectory && !other->isDirectory)
        {
            if (other->lastModificationDate<this->lastModificationDate)
                result.push_back(new Event(this->path, this->lastModificationDate, false, UPDATE_THIS));
            if (other->lastModificationDate>this->lastModificationDate)
                result.push_back(new Event(this->path, other->lastModificationDate, false, UPDATE_OTHER));
        }
        else if (!(this->isDirectory && other->isDirectory))
        {
            result.push_back(new Event(this->path, this->lastModificationDate, this->isDirectory, CREATE));
            result.push_back(new Event(other->path, other->lastModificationDate, other->isDirectory, DELETE));
        }
    }
    for (auto c : this->children)
    {
        childEvents = c->FindDifferences(fs);
        result.insert(result.end(), childEvents.begin(), childEvents.end());
    }
    if (parent == nullptr)
    {

        childEvents = this->FindDeletes(fs);
        result.insert(result.end(), childEvents.begin(), childEvents.end());
    }
    return result;
}

//znajduje usunięcia w tym systemie plików względem fs

deque<Event*> File::FindDeletes(File* f)
{
    deque<Event*> result, childEvents;
    File *other = this->GetFileFromPath(f->path.GetPath());
    if (other == nullptr) result.push_back(new Event(f->path, f->lastModificationDate, f->isDirectory, DELETE));
    for (auto c : f->children)
    {
        childEvents = this->FindDeletes(c);
        result.insert(result.end(), childEvents.begin(), childEvents.end());
    }
    return result;
}

File* File::GetChildByName(string &name)
{
    for (auto c : children)
    {
        if (c->name == name) return c;
    }
    return nullptr;
}