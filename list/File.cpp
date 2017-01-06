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

File::File()
{

}

File::File(string &path)
{
    this->path = path;
    this->UpdateLastModificationDate();
    parent = nullptr;
    name = Utility::GetFileNameFromPath(path);
    if (Utility::IsRegularFile(path))
    {
        isDirectory = false;
    }
    else
    {
        isDirectory = true;
        DIR * dir;
        dirent *dirEntries;
        if ((dir = opendir(path.c_str())) == NULL)
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
                File* child = new File(childPath);
                child->parent = this;
                children.push_back(child);
            }
        }
        closedir(dir);
    }
}

File::File(string path, time_t lastModificationDate, bool isDirectory)
{
    this->path = path;
    this->name = Utility::GetFileNameFromPath(path);
    this->lastModificationDate = lastModificationDate;
    this->isDirectory = isDirectory;
}

File::File(const File& orig)
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
    string result = "";
    result += path + " " + Utility::TimeToString(lastModificationDate);
    if (isDirectory) result += " d";
    else result += " n";
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

void File::FromString(string s)
{
    if (this->path != "") throw invalid_argument("You tried to load into nonempty node.");
    stringstream ss(s);
    string to;
    getline(ss, to, '\n');
    int space = to.find(" ");
    path = to.substr(0, space);
    lastModificationDate = stoi(to.substr(space + 1, to.size() - 3), nullptr);
    name = Utility::GetFileNameFromPath(path);
    if (to[to.size() - 1] == 'd') isDirectory = true;
    else isDirectory = false;

    string childString = "";
    while (getline(ss, to, '\n'))
    {
        childString += to.substr(1, to.size() - 1) + "\n";
        while (getline(ss, to, '\n'))
        {
            if (to[1] == ' ')
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
    }
    if (childString != "")
    {
        File* newChild = new File();
        newChild->FromString(childString);
        children.push_back(newChild);
    }
}

File* File::GetFileFromPath(string path)
{
    if (path[0] == '.' && path[1] == '/') path = path.substr(2, path.size() - 1);
    if (path[0] == '/') path = path.substr(1, path.size() - 1);
    int firstSlash = path.find("/");
    if (firstSlash != string::npos) path = path.substr(firstSlash + 1, path.size() - 1);
    if (path == this->name) return this;
    for (auto c : children)
    {
        if (path == c->name) return c;
        if (path.find(c->name) != string::npos)
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

void File::UpdateLastModificationDate()
{
    lastModificationDate = Utility::GetLastModificationDate(path);
}

//znajduje różnice w tym systemie plików względem fs

deque<Event*> File::FindDifferences(File* f)
{
    deque<Event*> result, childEvents;
    File *other = f->GetFileFromPath(this->path);
    if (other == nullptr) result.push_back(new Event(this->path, this->lastModificationDate, this->isDirectory, CREATE));
    else
    {
        if (!this->isDirectory && !other->isDirectory)
        {
            if (other->lastModificationDate<this->lastModificationDate)
                result.push_back(new Event(this->path, this->lastModificationDate, false, NEWER));
            if (other->lastModificationDate>this->lastModificationDate)
                result.push_back(new Event(this->path, other->lastModificationDate, false, OLDER));
        }
        else if (!(this->isDirectory && other->isDirectory))
        {
            result.push_back(new Event(this->path, this->lastModificationDate, this->isDirectory, CREATE));
            result.push_back(new Event(other->path, other->lastModificationDate, other->isDirectory, DELETE));
        }
    }
    for (auto c : this->children)
    {
        childEvents = c->FindDifferences(f);
        result.insert(result.end(), childEvents.begin(), childEvents.end());
    }
    if (parent == nullptr)
    {

        childEvents = this->FindDeletes(f);
        result.insert(result.end(), childEvents.begin(), childEvents.end());
    }
    return result;
}

//znajduje usunięcia w tym systemie plików względem fs

deque<Event*> File::FindDeletes(File* f)
{
    deque<Event*> result, childEvents;
    File *other = this->GetFileFromPath(f->path);
    if (other == nullptr) result.push_back(new Event(f->path, f->lastModificationDate, f->isDirectory, DELETE));
    for (auto c : f->children)
    {
        childEvents = this->FindDeletes(c);
        result.insert(result.end(), childEvents.begin(), childEvents.end());
    }
    return result;
}

void File::InsertFileAt(File* f, string path)
{
    string rest = "";
    string first = Utility::GetFirstElementOfPath(path);
    int startPos = 1;
    if (path[0] == '.' && path[1] == '/') startPos = 2;
    if (path != first)rest = path.substr(first.size() + startPos);
    if (rest.rfind("/") == 0)
    {
        first = rest.substr(1);
        if (GetChildByName(first) == nullptr)
        {
            f->parent = this;
            children.push_back(f);
        }
        else throw FileAlreadyExistsException(this->path + path);
    }
    else if (first == this->name)
    {
        string childName = Utility::GetFirstElementOfPath(rest);
        File *child = GetChildByName(childName);
        if (child == nullptr)
        {
            string newFilePath = this->path + "/" + childName;
            File *newFile = new File(newFilePath, f->lastModificationDate, true);
            newFile->parent = this;
            this->children.push_back(newFile);
            newFile->InsertFileAt(f, rest);
        }
        else
        {
            if (child->isDirectory)
            {
                child->InsertFileAt(f, rest);
            }
            else
            {
                throw FileIsNotDirectoryException(path);
            }
        }
    }
    else
    {
        if (parent == nullptr) throw FileOutsideRootDirectoryException(first);
    }

}

void File::DeleteFileAt(string &path)
{
    File *toDelete = this->GetFileFromPath(path);
    if (toDelete != nullptr)
    {
        if (toDelete->children.size() == 0)
        {
            File *parent = toDelete->parent;
            if (parent != nullptr)
            {
                auto it = parent->children.begin();
                for (auto it = parent->children.begin(); it != parent->children.end(); ++it)
                {
                    if ((*it)->name == toDelete->name)
                    {
                        parent->children.erase(it);
                        break;
                    }
                }
                delete toDelete;
            }
            else
            {
                throw runtime_error("There is something wrong with the way this file was inserted");
            }
        }
        else
        {
            throw FileHasChildrenException(path);
        }
    }
    else
    {
        throw FileDoesNotExistException(path);
    }
}

void File::ReplaceFile(File* f)
{

}

File* File::GetChildByName(string &name)
{
    for (auto c : children)
    {
        if (c->name == name) return c;
    }

    return nullptr;
}
