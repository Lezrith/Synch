/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Path.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on February 13, 2017, 12:22 PM
 */

#include <mutex>

#include "Path.h"

Path::~Path()
{
}

Path::Path(string path) :
path(path)
{
}

Path::Path() : Path("")
{

}

string Path::GetFilename()
{
    int slashIndex = path.rfind("/");
    return path.substr(slashIndex + 1);
}

string Path::GetFirstElement()
{
    int slashIndex = path.find("/");
    return path.substr(0, slashIndex);
}

string Path::GetParentDirectoryPath()
{
    int slashIndex = path.rfind("/");
    return path.substr(0, slashIndex);
}

string Path::TakeFirstElement()
{
    int slashIndex = path.find("/");
    string&& result = path.substr(0, slashIndex);
    path.replace(0, slashIndex + 1, "");
    return result;
}

string Path::ToString()
{
    return to_string(path.size()) + " " + path;
}

string Path::FromString(string s)
{
    int space = s.find(" ");
    try
    {
        int length = stoi(s.substr(0, space));
        this->path = s.substr(space + 1, length);
        return s.substr(space + length + 2);
    } catch (const invalid_argument &e)
    {
        cout << "This string is not a path: " + s << endl;
    }
    return "";
}
