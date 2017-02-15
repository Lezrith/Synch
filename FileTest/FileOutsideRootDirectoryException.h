/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FileOutsideRootDirectoryException.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 6, 2017, 8:05 AM
 */

#ifndef FILEOUTSIDEROOTDIRECTORYEXCEPTION_H
#define FILEOUTSIDEROOTDIRECTORYEXCEPTION_H
#include <string>
#include <stdexcept>
using namespace std;

class FileOutsideRootDirectoryException : public runtime_error
{
public:

    FileOutsideRootDirectoryException(string p) : runtime_error("You are trying to access files outside root directory"), path(p)
    {
    }

    virtual const char* what() const throw ()
    {
        string result = runtime_error::what();
        result += ": " + path;
        return result.c_str();
    }
private:
    string path;
};
#endif /* FILEOUTSIDEROOTDIRECTORYEXCEPTION_H */

