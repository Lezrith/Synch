/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Event.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 4:39 PM
 */

#ifndef EVENT_H
#define EVENT_H
#include <string>
using namespace std;
class File;

enum EventType
{
    CREATE,
    DELETE,
    NEWER,
    OLDER,
    MOVE,
    COPY
};

class Event
{
public:
    string path;
    string pathDst = "";
    time_t when;
    bool isDirectory; //potrzebna lepsza nazwa ;_;
    EventType type;

    Event(string path, string pathDst, time_t when, bool isDirectory, EventType type) :
    path(path), pathDst(pathDst), when(when), isDirectory(isDirectory), type(type)
    {
    }

    Event(string path, time_t when, bool isDirectory, EventType type) :
    path(path), when(when), isDirectory(isDirectory), type(type)
    {
    }

    string ToString();
    Event();
    Event(const Event& orig);
    virtual ~Event();
private:

};

#endif /* EVENT_H */

