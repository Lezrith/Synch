/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   Utility.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on January 3, 2017, 12:56 PM
 */

#include "Utility.h"

Utility::Utility()
{
}

Utility::Utility(const Utility& orig)
{
}

Utility::~Utility()
{
}

int Utility::IsRegularFile(string &path)
{
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) == -1)
    {
        cout << "Error(" << errno << ") opening " << path << endl;
        exit(EXIT_FAILURE);
    }
    return S_ISREG(path_stat.st_mode);
}

string Utility::GetFileNameFromPath(string& path)
{
    int lastSlashPosition = path.rfind("/");
    if (lastSlashPosition == -1) return path;
    else return path.substr(lastSlashPosition + 1, path.size() - 1);
}

string Utility::TimeToString(time_t& time)
{
    stringstream ss;
    ss << time;
    return ss.str();
}

time_t Utility::GetLastModificationDate(string& path)
{
    struct stat attrib;
    if (stat(path.c_str(), &attrib) == -1)
    {
        cout << "Error(" << errno << ") opening " << path << endl;
        exit(EXIT_FAILURE);
    }
    return attrib.st_mtime;
}

string Utility::GetFirstElementOfPath(string& path)
{
    int startPos = 0;
    if (path[0] == '.' && path[1] == '/') startPos = 2;
    if (path[0] == '/') startPos = 1;

    int firstSlash = path.substr(startPos).find("/");
    if (firstSlash != string::npos) return path.substr(startPos, firstSlash);
    else return path.substr(startPos);
}

ssize_t Utility::ReadDataFromSocket(int fd, char* buffer, ssize_t buffsize)
{
    cout << "Reading data from socket " << fd << endl;
    auto ret = read(fd, buffer, buffsize);
    if (ret == -1) error(1, errno, "read failed on descriptor %d", fd);
    return ret;
}

void Utility::WriteDataToScoket(int fd, char* buffer, ssize_t count)
{
    cout << "Writing data to socket " << fd << endl;
    auto ret = write(fd, buffer, count);
    if (ret == -1) error(1, errno, "write failed on descriptor %d", fd);
    if (ret != count) throw SentLessThenExcpectedException(fd, count, ret);
}

int Utility::OpenListeningSocket(char* ip, char* portc)
{
    auto port = readPort(portc);
    int servFd = socket(AF_INET, SOCK_STREAM, 0);
    if (servFd == -1) error(1, errno, "socket failed");

    // prevent dead sockets from throwing pipe errors on write
    signal(SIGPIPE, SIG_IGN);

    setReuseAddr(servFd);

    // bind to any address and port provided in arguments
    sockaddr_in serverAddr{.sin_family = AF_INET, .sin_port = htons((short) port), .sin_addr =
        {INADDR_ANY}};
    int res = bind(servFd, (sockaddr*) & serverAddr, sizeof (serverAddr));
    if (res) error(1, errno, "bind failed");

    // enter listening mode
    res = listen(servFd, 32);
    if (res) error(1, errno, "listen failed");
    cout << "Opened socket for " << ip << " " << port << " " << servFd << endl;
    return servFd;
}

void Utility::CloseSocket(int sd)
{
    cout << "Closing socket " << sd << endl;
    close(sd);
}

uint16_t Utility::readPort(char * txt)
{
    char * ptr;
    auto port = strtol(txt, &ptr, 10);
    if (*ptr != 0 || port < 1 || (port > ((1 << 16) - 1))) error(1, 0, "illegal argument %s", txt);
    return port;
}

void Utility::setReuseAddr(int sock)
{
    const int one = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one));
    if (res) error(1, errno, "setsockopt failed");
}

void Utility::SetSocketToNonBlocking(int sd)
{
    const int one = 1;
    int res = ioctl(sd, FIONBIO, &one);
    if (res < 0)
    {
        perror("ioctl() failed");
        close(sd);
        exit(-1);
    }
}

bool Utility::ValidateIP(char *ip)
{
    char buf[sizeof (in6_addr)];
    int res = inet_pton(AF_INET, ip, buf);
    if (res > 0) return true;
    else return false;
}

bool Utility::ValidatePath(string path)
{
    return true;
}

bool Utility::ValidatePort(string port)
{
    return true;
}
