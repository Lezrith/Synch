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
#include <sstream>
#include <algorithm>
#include <iterator>
#include <iostream>
#include "Path.h"
using namespace std;
class File;

enum EventType
{
    CREATE,
    DELETE,
    UPDATE_OTHER,
    UPDATE_THIS
};

class Event
{
public:
    Event(Path path, Path pathDst, time_t when, bool isDirectory, EventType type);
    Event(Path path, time_t when, bool isDirectory, EventType type);
    string ToString();
    void FromString(string s, int source);
    Event();
    Event(const Event& orig);
    virtual ~Event();
    int GetSource() const;
    Path GetPath() const;
    EventType GetType() const;
    bool IsDirectory() const;
private:
    Path path;
    Path pathDst;
    time_t when;
    bool isDirectory;
    int source;
    EventType type;
};

#endif /* EVENT_H */

