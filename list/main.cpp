/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   main.cpp
 * Author: Krzysztof Tomczak
 *
 * Created on January 2, 2017, 11:20 AM
 */

#include "main.h"
/*
 *
 */

string path = "./test";

int main(int argc, char** argv)
{
    string save = "./save";
    FileSystem* fs = new FileSystem(path);
    File *f = new File("./test/d/a.txt", 11, false);
    cout << fs->ToString() << endl;
    fs->InsertFile(f);
    string g = "./test/d/a.txt";
    fs->DeleteFileAt(g);
    g = "./test/d";
    fs->DeleteFileAt(g);
    cout << fs->ToString() << endl;
    delete fs;
    return 0;
}

