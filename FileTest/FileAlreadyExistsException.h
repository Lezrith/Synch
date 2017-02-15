/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FileAlreadyExistsException.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 5, 2017, 11:00 PM
 */

#ifndef FILEALREADYEXISTSEXCEPTION_H
#define FILEALREADYEXISTSEXCEPTION_H
#include <string>
#include <stdexcept>
using namespace std;

class FileAlreadyExistsException : public runtime_error
{
public:

    FileAlreadyExistsException(string p) : runtime_error("There is already a file with that path: "), path(p)
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
#endif /* FILEALREADYEXISTSEXCEPTION_H */

