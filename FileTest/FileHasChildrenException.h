/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FileHasChildrenException.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 6, 2017, 8:28 AM
 */

#ifndef FILEHASCHILDRENEXCEPTION_H
#define FILEHASCHILDRENEXCEPTION_H
#include <string>
#include <stdexcept>
using namespace std;

class FileHasChildrenException : public runtime_error
{
public:

    FileHasChildrenException(string p) : runtime_error("You are trying to delete a nonempty node"), path(p)
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
#endif /* FILEHASCHILDRENEXCEPTION_H */

