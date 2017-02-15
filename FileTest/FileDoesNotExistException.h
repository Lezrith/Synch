/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FileDoesNotExistException.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 6, 2017, 8:10 AM
 */

#ifndef FILEDOESNOTEXISTEXCEPTION_H
#define FILEDOESNOTEXISTEXCEPTION_H
#include <string>
#include <stdexcept>
#include "Path.h"
using namespace std;

class FileDoesNotExistException : public runtime_error
{
public:

    FileDoesNotExistException(string p) : runtime_error("File does not exist"), path(p)
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
#endif /* FILEDOESNOTEXISTEXCEPTION_H */

