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

string Event::ToString()
{
    string result = "";
    result += to_string(type) + " ";
    result += path.ToString() + " " + pathDst.ToString();
    result += to_string(when);
    if (isDirectory) result += " dir";
    return result;
}
