/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   ReplacingNewrFileException.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 7, 2017, 2:01 PM
 */

#ifndef REPLACINGNEWERFILEEXCEPTION_H
#define REPLACINGNEWERFILEEXCEPTION_H
#include <string>
#include <stdexcept>
using namespace std;

class ReplacingNewerFileException : public runtime_error
{
public:

    ReplacingNewerFileException(string p, time_t c, time_t o) : runtime_error("You tried to replace newer file with older"), path(p), current(c), older(o)
    {

    }

    virtual const char* what() const throw ()
    {
        string result = runtime_error::what();
        result += ": " + path + " ,time: " + to_string(current) + " with " + to_string(older);
        return result.c_str();
    }
private:
    string path;
    time_t older;
    time_t current;
};


#endif /* REPLACINGNEWERFILEEXCEPTION_H */

