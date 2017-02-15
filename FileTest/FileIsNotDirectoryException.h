/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FileIsNotDirectoryException.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 6, 2017, 8:01 AM
 */

#ifndef FILEISNOTDIRECTORYEXCEPTION_H
#define FILEISNOTDIRECTORYEXCEPTION_H
#include <string>
#include <stdexcept>
using namespace std;

class FileIsNotDirectoryException : public runtime_error
{
public:

    FileIsNotDirectoryException(string p) : runtime_error("One of the files in the path is not a directory"), path(p)
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
#endif /* FILEISNOTDIRECTORYEXCEPTION_H */

