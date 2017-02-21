#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string.h>
#include <thread>
#include <map>
#include <mutex>
#include <vector>
#include <math.h>
#include <ftw.h>
#include <chrono>
#include <assert.h>
#include "Network.h"
#include "FileSystem.h"

#define QUEUE_SIZE 16
using namespace std;

struct thread_info
{
    char data[BUFLEN];
    char data2[BUFLEN];
    int sock_num;
};

char *file_path;
vector<thread> threads;
map<int, mutex*> thread_mutexes;
map<std::thread::id, thread_info> thread_data;

/* function used when file or directory is cut from (MOVED_FROM) watched directory
   in this case it is should be deleted (directory is deleted together with its contents) */
/*int unlink_callback(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf){
    int rv = remove(fpath);
    if (rv)
        perror(fpath);
    return rv;
}*/

/* forwards the message to all clients except the client from whom server received it */
int forward_message(thread::id thread_id, string debug_msg, string short_msg,
                    string file, thread_info *currentThreadInfo){
    int return_value = 0;
    if(short_msg == "FILCREATE" || short_msg == "FILMODIFY")
        assert(file != "");
    else
        assert(file == "");
    for (auto it = thread_data.begin(); it != thread_data.end(); it++) {
        if(it->first != thread_id){
            cout << debug_msg << it->second.sock_num << endl;
            thread_mutexes.at(it->second.sock_num)->lock();

            if(short_msg != "FILMODIFY"){
                send_string(it->second.sock_num, short_msg, short_msg + "_MSG");
                send_ch_arr(it->second.sock_num, currentThreadInfo->data, short_msg + "_DATA", 0);

                if(short_msg == "MOVED")
                    send_ch_arr(it->second.sock_num, currentThreadInfo->data2, short_msg + "_DATA_2", 0);
                if(short_msg == "FILCREATE")
                    sendLastModificationDate(it->second.sock_num, file);
            } else {
                return_value = send_file(it->second.sock_num, file.c_str(), currentThreadInfo->data);
            }

            thread_mutexes.at(it->second.sock_num)->unlock();
        }
        if(return_value == -1)
            break;
    }
    return return_value;
}

void handle_client(int client_sock_num, string root_path_string){
    thread::id thread_id = this_thread::get_id();
    thread_info ti;
    ti.sock_num = client_sock_num;
    mutex mx;
    thread_data.insert(pair<std::thread::id, thread_info>(thread_id, ti));
    thread_mutexes.insert(pair<int, mutex*>(client_sock_num, &mx));
    thread_info *currentThreadInfo = &thread_data.at(thread_id);
    cout << "Thread " << thread_id << " started running." << endl;
    char file[BUFLEN];
    char message_type[BUFLEN];
    struct stat helper;
    char root_path[BUFLEN];
    strncpy(root_path, root_path_string.c_str(), BUFLEN);

    /* Receive client's message about connection type */
    bool performInitialSynchronization = false;
    if (receive_message(currentThreadInfo->sock_num, currentThreadInfo->data) < 0)
        cout << "Connection type message: failed for client " << thread_id << endl;
    else
    {
        //first time connection - perform initial synch
        if (!strcmp(currentThreadInfo->data, "NORMALCONNECTION"))
        {
            cout << "Connection type message: client " << thread_id << " connected normally." << endl;
            performInitialSynchronization = true;
        }
        //reconnection - no need for initial synch
        else if (!strcmp(currentThreadInfo->data, "RECONNECTED"))
        {
            cout << "Connection type message: client " << thread_id << " reconnected." << endl;
            performInitialSynchronization = false;
        }
        else
            cout << "Connection type message: unknown type of connection for client " << thread_id << endl;
    }
    if (performInitialSynchronization)
    {
        /* Wait for initial synch */
        if (receive_message(currentThreadInfo->sock_num, currentThreadInfo->data) < 0)
            cout << "Initial synchronization: failed for client " << thread_id << endl;
        else
        {
            //get client's FileSystem in form of string
            string clientFSString = receive_big_string(client_sock_num);
            if ( clientFSString == "")
                cout << "Initial synchronization: failed for client " << thread_id << endl;
            else
            {
                cout << "Initial synchronization for client " << thread_id << endl;
                //create list of server's files
                FileSystem *serverFS = new FileSystem(root_path_string);
                FileSystem *clientFS = new FileSystem;
                clientFS->FromString(clientFSString);
                cout << "Files on server:" << endl << serverFS->ToString() << endl;
                cout << "Files on client:" << endl << clientFS->ToString() << endl;
                //check what needs to be done on client's side
                auto changes = serverFS->FindDifferences(clientFS);
                //loop through changes in reverse order so deletes happen from the 'bottom'
                for (deque<Event*>::reverse_iterator i = changes.rbegin(); i != changes.rend(); ++i)
                {
                    /* There is a file on client but not on server */
                    /* Should be handled the same way as DIRDELETE, FILDELETE from other client */
                    if ((*i)->GetType() == DELETE)
                    {
                        cout << (*i)->ToString() << endl;
                        if ((*i)->IsDirectory())
                        {
                            //ask client to delete directory
                            thread_mutexes.at(currentThreadInfo->sock_num)->lock();
                            send_string(currentThreadInfo->sock_num, "DIRDELETE", "DIRDELETE_MSG");
                            send_string(currentThreadInfo->sock_num, (*i)->GetPath().GetPath(), "DIRDELETE_DATA");
                            thread_mutexes.at(currentThreadInfo->sock_num)->unlock();
                        }
                        else
                        {
                            //ask client to delete file
                            thread_mutexes.at(currentThreadInfo->sock_num)->lock();
                            send_string(currentThreadInfo->sock_num, "FILDELETE", "FILDELETE_MSG");
                            send_string(currentThreadInfo->sock_num, (*i)->GetPath().GetPath(), "FILDELETE_DATA");
                            thread_mutexes.at(currentThreadInfo->sock_num)->unlock();
                        }
                    }
                }
                //loop through changes in normal order so creates happen from the 'top'
                for (auto c : changes)
                {
                    cout << c->ToString() << endl;
                    switch (c->GetType())
                    {
                        /* There is file on server and not on client*/
                        /* Should be handled the same way as FILCREATE, DIRCREATE and FILMODIFY from other client*/
                        case CREATE:
                            if (c->IsDirectory())
                            {
                                //ask client to create directory
                                thread_mutexes.at(currentThreadInfo->sock_num)->lock();
                                send_string(currentThreadInfo->sock_num, "DIRCREATE", "DIRCREATE_MSG");
                                send_string(currentThreadInfo->sock_num, c->GetPath().GetPath(), "DIRCREATE_DATA");
                                thread_mutexes.at(currentThreadInfo->sock_num)->unlock();
                            }
                            else
                            {
                                strcpy(currentThreadInfo->data, c->GetPath().GetPath().c_str());
                                snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
                                
                                //ask client to create file
                                thread_mutexes.at(currentThreadInfo->sock_num)->lock();
                                send_string(currentThreadInfo->sock_num, "FILCREATE", "FILCREATE_MSG");
                                send_string(currentThreadInfo->sock_num, c->GetPath().GetPath(), "FILCREATE_DATA");
                                sendLastModificationDate(currentThreadInfo->sock_num, file);
                                thread_mutexes.at(currentThreadInfo->sock_num)->unlock();

                                //ask client to modify file to match server's
                                thread_mutexes.at(currentThreadInfo->sock_num)->lock();
                                send_file(currentThreadInfo->sock_num, file, currentThreadInfo->data);
                                sendLastModificationDate(currentThreadInfo->sock_num, file);
                                thread_mutexes.at(currentThreadInfo->sock_num)->unlock();
                            }
                            break;

                        case DELETE:
                        /* Ignore because it was handled earlier */
                            cout << "Handled" << endl;
                            break;

                        /* There is newer version of file on server */
                        /* Should be handled the same way as DIRDELETE, FILDELETE from other client */
                        case UPDATE_THIS:
                            strcpy(currentThreadInfo->data, c->GetPath().GetPath().c_str());
                            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);

                            //ask client to receive new version of file
                            thread_mutexes.at(currentThreadInfo->sock_num)->lock();
                            send_file(currentThreadInfo->sock_num, file, currentThreadInfo->data);
                            thread_mutexes.at(currentThreadInfo->sock_num)->unlock();
                            break;

                        /* There is newer version of file on client */
                        case UPDATE_OTHER:
                            strcpy(currentThreadInfo->data, c->GetPath().GetPath().c_str());
                            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
                            
                            /* Ask client to send file */
                            thread_mutexes.at(currentThreadInfo->sock_num)->lock();
                            send_string(currentThreadInfo->sock_num, "SENDFILE", "SENDFILE_MSG");
                            send_string(currentThreadInfo->sock_num, c->GetPath().GetPath(), "SENDFILE_DATA");
                            thread_mutexes.at(currentThreadInfo->sock_num)->unlock();
                            break;

                        /* Event not recognized or not implemented */
                        default:
                            cout << "Event not recognized or not implemented." << endl;
                            break;
                    }
                }
                changes.clear();

                delete serverFS;
                delete clientFS;
            }
        }
    }

    while(1){
        printf("===============================\n");
        if(receive_message(client_sock_num, currentThreadInfo->data) < 0)
            break;
        strncpy(message_type, currentThreadInfo->data, BUFLEN);
        printf("Message from socket %d\n", currentThreadInfo->sock_num);

        if(!strcmp(message_type, "DIRCREATE")){
            if(receive_message(client_sock_num, currentThreadInfo->data) < 0)
                break;
            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
            printf("Received: create directory %s\n", file);
            if (stat(file, &helper) < 0){
                mkdir(file, 0755);
                forward_message(thread_id, "Sending directory creation to client ", "DIRCREATE", "", currentThreadInfo);
            } else
                printf("Directory already exists!\n");
        } 

        else if(!strcmp(message_type, "FILCREATE")){
            if(receive_message(client_sock_num, currentThreadInfo->data) < 0)
                break;
            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
            printf("Received: create file %s\n", file);
            if (stat(file, &helper) < 0){
                int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                close(fd);
                time_t lmd = receiveModificationDate(currentThreadInfo->sock_num);
                Utility::UpdateLastModificationDate(file, lmd);
                forward_message(thread_id, "Sending file creation to client ", "FILCREATE", file, currentThreadInfo);
            } else
                printf("File already exists!\n");
        } 

        else if(!strcmp(message_type, "FILDELETE")){
            if(receive_message(client_sock_num, currentThreadInfo->data) < 0)
                break;
            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
            printf("Received: delete file %s\n", file);
            if (stat(file, &helper) == 0) {
                forward_message(thread_id, "Sending file deletion to client ", "FILDELETE", "", currentThreadInfo);
                unlink(file);
            } else
                printf("File doesn't exist!\n");
        } 

        else if(!strcmp(message_type, "DIRDELETE")){
            if(receive_message(client_sock_num, currentThreadInfo->data) < 0)
                break;
            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
            printf("Received: delete directory %s\n", file);
            if (stat(file, &helper) == 0){
                forward_message(thread_id, "Sending directory deletion to client ", "DIRDELETE", "", currentThreadInfo);
                rmdir(file);
            } else
                printf("Directory doesn't exist!\n");
        }

        /* this message means that someone cut (MOVED_FROM) file or directory from watched directory
           so we have to delete it and tell other clients to do the same */
        /*else if(!strcmp(message_type, "BIGDELETE")){
            if(receive_message(currentThreadInfo->sock_num, currentThreadInfo->data) < 0)
                break;
            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
            printf("Received: delete %s (file or directory with its contents)\n", file);*/

            /* delete file or contents of directory, FTW_DEPTH means a post-order traversal
               so files inside directories will be deleted first */
            /*nftw(file, unlink_callback, 256, FTW_DEPTH | FTW_PHYS);
            
            forward_message(thread_id, "Sending file or whole directory deletion to client ", "BIGDELETE", "", currentThreadInfo);
        }*/

        else if(!strcmp(message_type, "MOVED")){
            if(receive_message(currentThreadInfo->sock_num, currentThreadInfo->data) < 0)
                break;
            if(receive_message(currentThreadInfo->sock_num, currentThreadInfo->data2) < 0)
                break;
            char file_2[BUFLEN];
            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
            snprintf(file_2, BUFLEN, "%s%s", root_path, currentThreadInfo->data2);
            printf("Received: move file %s to %s\n", file, file_2);
            if((stat(file, &helper) == -1) || (stat(file_2, &helper) == 0))
                printf("Source file doesn't exist or destination file exists!");
            else {
                rename(file, file_2);
                forward_message(thread_id, "Sending file moving / renaming to client ", "MOVED", "", currentThreadInfo);
            }
        } else if(!strcmp(message_type, "FILMODIFY")){
            int result;
            if(receive_message(currentThreadInfo->sock_num, currentThreadInfo->data) < 0)
                break;
            snprintf(file, BUFLEN, "%s%s", root_path, currentThreadInfo->data);
            printf("Received: modify file %s\n", file);

            time_t lmd = receiveModificationDate(currentThreadInfo->sock_num);
            receive_file(currentThreadInfo->sock_num, file, NULL, false, NULL);
            if(lmd!=-1)
                Utility::UpdateLastModificationDate(file, lmd);

            result = forward_message(thread_id, "Sending file modification to client ", "FILMODIFY", file, currentThreadInfo);
            if(result == -1)
                break;
        }
    }
    close(client_sock_num);
    auto it = thread_data.find(thread_id);
    thread_data.erase(it);
    cout << "Thread " << thread_id << " stopped running" << endl;
}

int main(int argc, char **argv) {
    if(argc<4) {
        printf("Usage: %s PATH IP PORT\n",argv[0]);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in temp;
    if(inet_pton(AF_INET, argv[2], &(temp.sin_addr)) == 0){
        printf("Not a valid IP address.\n");
        exit(EXIT_FAILURE);
    }
    int port_num = atoi(argv[3]);
    if(!(port_num > 1023 && port_num < 65536)){
        printf("Not allowed port number.\n");
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in soc_add_server, stClientAddr;
	soc_add_server.sin_family = AF_INET;
	soc_add_server.sin_addr.s_addr = inet_addr(argv[2]);
	soc_add_server.sin_port = htons(port_num);
	
	int sock_num;
	if((sock_num = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(EXIT_FAILURE);
    }
    
    int enable = 1;
    if(setsockopt(sock_num, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt");
        
    if(bind(sock_num, (struct sockaddr*) &soc_add_server, sizeof(soc_add_server)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if(listen(sock_num, QUEUE_SIZE) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }
    socklen_t nTmp = sizeof(struct sockaddr);
    char root_path[BUFLEN];
    
    /* remove slash from the end of root directory */
    for (int i=0; i<BUFLEN; i++){
        if(argv[1][i] == 0){
            if(root_path[i-1] == '/')
                root_path[i-1] = 0;
            else
                root_path[i] = argv[1][i];
            break;
        } else
            root_path[i] = argv[1][i];
    }
    printf("Server directory is %s\n", root_path);
        
    while(1) {
        int client_sock_num = accept(sock_num, (struct sockaddr*)&stClientAddr, &nTmp);
        if(client_sock_num < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("Connection from %s, port: %d\n", inet_ntoa((struct in_addr)stClientAddr.sin_addr), stClientAddr.sin_port);
        printf("Port number is %d\n", client_sock_num);

        /* create new thread which listens to one client */        
        threads.push_back(thread(handle_client, client_sock_num, string(root_path)));
        
    }
    close(sock_num);
}
