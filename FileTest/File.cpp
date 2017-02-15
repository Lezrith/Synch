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

File::File() : File("", 0, nullptr)
{

}

File::File(string &path) : path(path)
{
    this->UpdateLastModificationDate();
    parent = nullptr;
    name = this->path.GetFilename();
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
    if (this->path.ToString() != "") throw invalid_argument("You tried to load into nonempty node.");
    stringstream ss(s);
    string to;
    getline(ss, to, '\n');
    int space = to.find(" ");
    path = to.substr(0, space);
    lastModificationDate = stoi(to.substr(space + 1, to.size() - 3), nullptr);
    name = path.GetFilename();
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

File* File::GetFileFromPath(Path path)
{
    path.TakeFirstElement();
    if (path.ToString() == this->name) return this;
    for (auto c : children)
    {
        if (path.ToString() == c->name) return c;
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

void File::UpdateLastModificationDate()
{
    string &&p = path.ToString();
    lastModificationDate = Utility::GetLastModificationDate(p);
}

//znajduje różnice w tym systemie plików względem fs

deque<Event*> File::FindDifferences(File* f)
{
    deque<Event*> result, childEvents;
    string &&p = path.ToString();
    File *other = f->GetFileFromPath(p);
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
    File *other = this->GetFileFromPath(f->path.ToString());
    if (other == nullptr) result.push_back(new Event(f->path, f->lastModificationDate, f->isDirectory, DELETE));
    for (auto c : f->children)
    {
        childEvents = this->FindDeletes(c);
        result.insert(result.end(), childEvents.begin(), childEvents.end());
    }
    return result;
}

void File::InsertFileAt(File* f, Path path)
{
    string first = path.TakeFirstElement();
    if (path.GetFilename() == path.GetFirstElement())
    {
        first = path.GetFirstElement();
        if (GetChildByName(first) == nullptr)
        {
            f->parent = this;
            children.push_back(f);
        }
        else throw FileAlreadyExistsException(this->path.ToString() + path.ToString());
    }
    else if (first == this->name)
    {
        string childName = path.GetFirstElement();
        File *child = GetChildByName(childName);
        if (child == nullptr)
        {
            string newFilePath = this->path.ToString() + "/" + childName;
            File *newFile = new File(newFilePath, f->lastModificationDate, true);
            newFile->parent = this;
            this->children.push_back(newFile);
            newFile->InsertFileAt(f, path);
        }
        else
        {
            if (child->isDirectory)
            {
                child->InsertFileAt(f, path);
            }
            else
            {
                throw FileIsNotDirectoryException(path.ToString());
            }
        }
    }
    else
    {
        if (parent == nullptr) throw FileOutsideRootDirectoryException(first);
    }

}

void File::DeleteFileAt(Path &path)
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
            throw FileHasChildrenException(path.ToString());
        }
    }
    else
    {
        throw FileDoesNotExistException(path.ToString());
    }
}

void File::ReplaceFile(File* f)
{
    File *toReplace = this->GetFileFromPath(f->path.ToString());
    if (toReplace != nullptr)
    {
        if (toReplace->lastModificationDate <= f->lastModificationDate)
        {
            toReplace->lastModificationDate = f->lastModificationDate;
            delete f;
            f = toReplace;
        }
        else
        {
            throw ReplacingNewerFileException(f->path.ToString(), toReplace->lastModificationDate, f->lastModificationDate);
        }
    }
    else
    {
        throw FileDoesNotExistException(f->path.ToString());
    }
}

File* File::GetChildByName(string &name)
{
    for (auto c : children)
    {
        if (c->name == name) return c;
    }
    return nullptr;
}
