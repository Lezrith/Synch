/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Event.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 4:39 PM
 */

#include <stdexcept>
#include <vector>

#include "Event.h"

Event::Event() : path(""), pathDst("")
{
}

Event::Event(const Event& orig) : path(orig.path), pathDst(orig.pathDst)
{
}

Event::~Event()
{
}

Event::Event(Path path, Path pathDst, time_t when, bool isDirectory, EventType type) :
path(path), pathDst(pathDst), when(when), isDirectory(isDirectory), type(type)
{
}

Event::Event(Path path, time_t when, bool isDirectory, EventType type) :
path(path), pathDst(""), when(when), isDirectory(isDirectory), type(type)
{
}

string Event::ToString()
{
    string result = "";
    result += to_string(type) + " ";
    result += path.ToString() + " ";
    result += to_string(when);
    if (isDirectory) result += " dir";
    return result;
}

void Event::FromString(string s, int source)
{
    this->source = source;
    int nextSpace = s.find(" ");
    try
    {
        this->type = static_cast<EventType> (stoi(s.substr(0, nextSpace)));
    } catch (const invalid_argument &e)
    {
        cout << "Tried to make an event from string which does not represent an event: " + s << endl;
    }
    s = s.substr(nextSpace + 1);

    s = this->path.FromString(s);
    nextSpace = s.find(" ");
    try
    {
        this->when = stoi(s.substr(0, nextSpace));
    } catch (const invalid_argument &e)
    {
        cout << "Tried to make an event from string which does not represent an event: " + s << endl;
    }
    if (s.find("dir") != string::npos)
    {
        this->isDirectory = true;
    }
    else
    {
        this->isDirectory = false;
    }
}

int Event::GetSource() const
{
    return source;
}

Path Event::GetPath() const
{
    return path;
}

EventType Event::GetType() const
{
    return type;
}

bool Event::IsDirectory() const
{
    return isDirectory;
}