/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   SentLessThenExpectedException.h
 * Author: Krzysztof Tomczak
 *
 * Created on January 8, 2017, 5:17 PM
 */

#ifndef SENTLESSTHENEXPECTEDEXCEPTION_H
#define SENTLESSTHENEXPECTEDEXCEPTION_H
#include <string>
#include <stdexcept>
using namespace std;

class SentLessThenExcpectedException : public runtime_error
{
public:

    SentLessThenExcpectedException(int fd, int count, int sent) :
    runtime_error("Wrote less than requested to descriptor"), fd(fd), count(count), sent(sent)
    {
    }

    virtual const char* what() const throw ()
    {
        string result = runtime_error::what();
        result += ": " + to_string(fd) + " " + to_string(sent) + " instead of " + to_string(count);
        return result.c_str();
    }
private:
    int fd;
    int count;
    int sent;
};


#endif /* SENTLESSTHENEXPECTEDEXCEPTION_H */

