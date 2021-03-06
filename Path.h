/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Path.h
 * Author: Krzysztof Tomczak
 *
 * Created on February 13, 2017, 12:22 PM
 */

#ifndef PATH_H
#define PATH_H
#include <string>
#include <iostream>

using namespace std;

class Path
{
public:
    Path(string path);
    Path();
    virtual ~Path();
    string GetFirstElement();
    string GetFilename();
    string GetParentDirectoryPath();
    string TakeFirstElement();
    string ToString();
    string FromString(string s);

    string GetPath() const
    {
        return path;
    }

private:
    string path;
};

#endif /* PATH_H */

