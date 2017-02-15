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
#include "Path.h"
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

    Event(Path path, Path pathDst, time_t when, bool isDirectory, EventType type) :
    path(path), pathDst(pathDst), when(when), isDirectory(isDirectory), type(type)
    {
    }

    Event(Path path, time_t when, bool isDirectory, EventType type) :
    path(path), pathDst(""), when(when), isDirectory(isDirectory), type(type)
    {
    }

    string ToString();
    Event();
    Event(const Event& orig);
    virtual ~Event();
private:
    Path path;
    Path pathDst;
    time_t when;
    bool isDirectory;
    int source;
    EventType type;
};

#endif /* EVENT_H */

