#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <ftw.h>
#include <thread>
#include <set>
#include <signal.h>
#include <chrono>
#include <mutex>
#include "Network.h"
#include "FileSystem.h"

#define MAX_DIRS 4096
#define MAX_RENAMED_FILES 32
#define FLAGS IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO

using namespace std;
const string SAVED_FILESYSTEM_SUFFIX = ".savedfs";

struct renamed_file_struct {
    int cookie;
    char old_name[BUFLEN];
} renamed_files[MAX_RENAMED_FILES];
int sock_num;
int root_length;
char root_directory[BUFLEN];
char **dir_list;
bool dir_list_state[MAX_DIRS];
char databuf[BUFLEN];
map<string, string> moveto_special;
map<string, string> moveto_normal;
set<string> inotify_ignore;
set<string> inotify_ignore_modified;
struct sockaddr_in soc_add;
int *watch_desc;
int in_file_desc;
mutex sendMutex;

/* function used when file or directory is cut from (MOVED_FROM) watched directory
   in this case it should be deleted (directory is deleted together with its contents) */
/*int unlink_callback(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf){*/
    /* information about deleted file or directory shouldn't be propagated to server */
    /*inotify_ignore.insert(string(fpath));
    int rv = remove(fpath);
    if (rv)
        perror(fpath);
    return rv;
}*/

/* points to an index where new watch can be created */
int find_free_index(){
    int dir_list_index = -1;
    for(int l=0; l<MAX_DIRS; l++){
        if(!dir_list_state[l]){
            dir_list_index = l;
            break;
        }
    }
    if(dir_list_index == -1){
        printf("Error: Too many dirs!\n");
        exit(EXIT_FAILURE);
    }
    return dir_list_index;
}

/* helper function for traversing directory tree - adds a watch to one directory */
int add_wat(const char *filepath, const struct stat *info, 
                const int typeflag, struct FTW *pathinfo){
    int index = find_free_index();
    strncpy(dir_list[index], filepath, BUFLEN);
    if(typeflag != FTW_D)
        return 0;

    for(int l = 0; ; l++) {
        if(l == BUFLEN-1){
            printf("Warning: possible buffer overflow.\n");
            break;
        }
        if(dir_list[index][l] == 0) {
            /* if there is a slash at the end of path 
            (especially root dir path given as program's argument), remove it */
            if(dir_list[index][l-1] == '/')
                dir_list[index][l-1] = 0;
                if(index == 0)
                    root_length = l;
            else
                dir_list[index][l] = 0;
            break;
        }
    }
    
    /* add a watch to this dir */
    watch_desc[index] = inotify_add_watch(in_file_desc, dir_list[index], FLAGS);
    dir_list_state[index] = true;
    if (watch_desc[index] == -1) {
        fprintf(stderr, "Cannot watch '%s'\n", dir_list[index]);
        perror("inotify_add_watch");
        return 0;
    }
    
    printf("Added watch %d to %s.\n", index, dir_list[index]);
    return 0;
}

/* finds all subdirectories of dir and adds watches for them */
void add_watches_to_subdirs(char *dir){

    int result;
    result = nftw(dir, add_wat, 0, 0);
    if(result < 0)
        perror("nftw");
}

bool check_if_file_ignored(char *path){
    set<string>::iterator in_ignore_it;
    in_ignore_it = inotify_ignore.find(string(path));
    if(in_ignore_it != inotify_ignore.end()){
        printf("This file should be ignored.\n");
        inotify_ignore.erase(in_ignore_it);
        return true;
    }
    return false;
}

bool check_if_modified_file_ignored(char *path){
    set<string>::iterator in_ignore_it;
    in_ignore_it = inotify_ignore_modified.find(string(path));
    if(in_ignore_it != inotify_ignore_modified.end()){
        printf("This modified file should be ignored.\n");
        inotify_ignore_modified.erase(in_ignore_it);
        return true;
    }
    return false;
}

void modify_file(char *full_path_arg, char *path_arg){
    sendMutex.lock();
    if(!check_if_modified_file_ignored(path_arg))
        send_file(sock_num, full_path_arg, path_arg);
    sendMutex.unlock();
    /*map<string, string>::iterator it;
    map<string, string>::iterator it2;
    it = moveto_special.find(string(full_path_arg));*/
    
    /* this is a file which was MOVED_TO, rename it to original name */
    /*if(it != moveto_special.end()){

        printf("Special renaming: moving %s to %s.\n", it->first.c_str(), it->second.c_str());
        rename(it->first.c_str(), it->second.c_str());
        
        it2 = moveto_normal.find(it->second);
        printf("Searching for %s in moveto_normal.\n", it2->second.c_str());
        if(it2 != moveto_normal.end()){
            printf("Erasing %s from moveto_normal.\n", it2->first.c_str());
            moveto_normal.erase(it2);
        }
        moveto_special.erase(it);
    }*/
}

void add_new_watch(char *full_path_arg){
    int dir_list_index = find_free_index();
    dir_list_state[dir_list_index] = true;
    
    strncpy(dir_list[dir_list_index], full_path_arg, BUFLEN);
    watch_desc[dir_list_index] = inotify_add_watch(in_file_desc, dir_list[dir_list_index], FLAGS);
        if (watch_desc[dir_list_index] == -1) {
            fprintf(stderr, "Cannot watch '%s'\n", dir_list[dir_list_index]);
            perror("inotify_add_watch");
            return;
        }
        
    printf("Adding watch %d to directory %s\n", dir_list_index, dir_list[dir_list_index]);
}

void create_dir(char *full_path_arg, char *path_arg){

    if(!check_if_file_ignored(path_arg)){
        sendMutex.lock();
        send_string(sock_num, "DIRCREATE", "DIRCREATE_MSG");
        send_ch_arr(sock_num, path_arg, "DIRCREATE_DATA", 0);
        sendMutex.unlock();
    }
    
    add_new_watch(full_path_arg);
}

void create_file(char *full_path_arg, char *path_arg){
    if(!check_if_file_ignored(path_arg)){
        sendMutex.lock();
        send_string(sock_num, "FILCREATE", "FILCREATE_MSG");
        send_ch_arr(sock_num, path_arg, "FILCREATE_DATA", 0);
        sendLastModificationDate(sock_num, full_path_arg);
        sendMutex.unlock();
    }
}

void delete_dir(char *full_path_arg, char *path_arg){

    /* mark the place of old watch as free */
    for(int k=0; k<MAX_DIRS; k++)
        if(!strcmp(full_path_arg, dir_list[k]))
            dir_list_state[k] = false;

    /* BIGDELETE message adds full path to set, so we have to search for full_path_arg too */
    if(!check_if_file_ignored(path_arg) && !(check_if_file_ignored(full_path_arg))){
        sendMutex.lock();
        send_string(sock_num, "DIRDELETE", "DIRDELETE_MSG");
        send_ch_arr(sock_num, path_arg, "DIRDELETE_DATA", 0);
        sendMutex.unlock();
    }
}

void delete_file(char *full_path_arg, char *path_arg){
    /* BIGDELETE message adds full path to set, so we have to search for full_path_arg too */
    if(!check_if_file_ignored(path_arg) && !(check_if_file_ignored(full_path_arg))){
        sendMutex.lock();
        send_string(sock_num, "FILDELETE", "FILDELETE_MSG");
        send_ch_arr(sock_num, path_arg, "FILDELETE_DATA", 0);
        sendMutex.unlock();
    }
}

void move_from(char *full_path_arg, char *path_arg, char *ptr_arg,
               const struct inotify_event *event_arg, char *buf_ptr, ssize_t len_arg){

    char *ptr_temp;
    const struct inotify_event *event_temp;
    int file_number = -1;
    
    /* check if this event has corresponding IN_MOVED_TO in the buffer
    this is ALMOST always the case when renaming files */
    ptr_temp = ptr_arg;
    event_temp = event_arg;
    //bool renaming = false;
    while(ptr_temp < buf_ptr + len_arg){
        ptr_temp += sizeof(struct inotify_event) + event_temp->len;
        event_temp = (const struct inotify_event *) ptr_temp;
        if(event_temp->cookie == event_arg->cookie) {
            printf("Corresponding cookie %d found in the buffer!\n", event_temp->cookie);
            //renaming = true;
        }
    }
    
    /* no cookie found, this is probably permanent deletion,
    server will have to deal with it on its own */
    /*if(!renaming) {
        printf("No cookie found!\n");
        send_string(sock_num, "BIGDELETE", "BIGDELETE_MSG");
        send_ch_arr(sock_num, path_arg, "BIGDELETE_DATA", 0);
        
        for(int k=0; k<MAX_DIRS; k++)
            if(!strcmp(full_path_arg, dir_list[k]))
                dir_list_state[k] = false;
    }*/
    
    //else {
    
    /* find a place to save a cookie and old filename */
    for(int j=0; j<MAX_RENAMED_FILES; j++)
        if(renamed_files[j].cookie == -1) { 
            file_number = j;
            break;
        }
    if(file_number == -1){
        printf("Can't find a place to save cookie!\n");
        file_number = 0;
    }
    renamed_files[file_number].cookie = event_arg->cookie;
    strncpy(renamed_files[file_number].old_name, path_arg, BUFLEN);
    //}
}

void move_to(char *full_path_arg, char *path_arg, const struct inotify_event *event_arg){
    //bool found_corresp_cookie = false;
    
    for(int j=0; j<MAX_RENAMED_FILES; j++) {
    
        /* we found which IN_MOVED_FROM event corresponds to current one */
        if(event_arg->cookie == (unsigned)renamed_files[j].cookie) {
            renamed_files[j].cookie=-1;
            //found_corresp_cookie = true;
            
            /* send changes to server only if these changes didn't come from it */
            if(!check_if_file_ignored(path_arg)){
                sendMutex.lock();
                send_string(sock_num, "MOVED", "MOVED_MSG");
                send_ch_arr(sock_num, renamed_files[j].old_name, "MOVED_FROM", 0);
                send_ch_arr(sock_num, path_arg, "MOVED_TO", 0);
                sendMutex.unlock();
            }
            
            /* deal with outdated watches */
            if(event_arg->mask & IN_ISDIR){
                char *outdated_dir;
                int result;
                char *dir_list_slash;
                char *old_name_slash;
                dir_list_slash = (char *) calloc(BUFLEN+1, sizeof(char));
                old_name_slash = (char *) calloc(BUFLEN+1, sizeof(char));
                if(!dir_list_slash || !old_name_slash)
                    perror("calloc");
                printf("Dealing with outdated watch: %s.\n", renamed_files[j].old_name);
                for(int k=0; k<MAX_DIRS; k++){
                
                    /* if we don't add slash then such situation may happen:
                    dir1/dir2 changed name to dir1/dir3 and watches for
                    dir1/dir2 should be removed but if there is dir1/dir2222 
                    then strstr will say that old name = dir1/dir2 is 
                    a substring of dir1/dir2222, so watches for dir1/dir2222 
                    are going to be removed as well - but the latter didn't change name!
                    on the other hand dir1/dir2/ is not a substring of dir1/dir2222/ */
                    
                    strncpy(dir_list_slash, dir_list[k], BUFLEN);
                    strcat(dir_list_slash, "/");
                    strncpy(old_name_slash, renamed_files[j].old_name, BUFLEN);
                    strcat(old_name_slash, "/");
                    outdated_dir = strstr(dir_list_slash, old_name_slash);
                    if(outdated_dir) {
                        printf("Removing watch %s.\n", outdated_dir);
                        result = inotify_rm_watch(in_file_desc, watch_desc[k]);
                        if(result < 0) perror("inotify_rm_watch");
                        else dir_list_state[k] = false;
                    }
                }
                free(dir_list_slash);
                free(old_name_slash);
                add_watches_to_subdirs(full_path_arg);
            }
            break;
        }
    }
    
    /* no corresponding cookie found - file was moved into watched directory;
    copy it so it's appearance will be detected */
    /*if(!found_corresp_cookie){
        char moved_special_name[BUFLEN];
        
        snprintf(moved_special_name, BUFLEN, "%sXXXXXX", full_path_arg);
        int fd_out = mkstemp(moved_special_name);
        if(fd_out < 0){
            perror("mkstemp");
            return;
        }
        
        int fd_in = open(full_path_arg, O_RDONLY);
        if(fd_in < 0){
            char err_msg[BUFLEN];
            snprintf(err_msg, BUFLEN, "Error while opening %s", full_path_arg);
            perror(err_msg);
            close(fd_out);
            return;
        }

        char buffer[16384];
        int read_from_input;
        while((read_from_input = read(fd_in, buffer, 16384)) > 0)
            write(fd_out, buffer, read_from_input);
        
        close(fd_out);
        close(fd_in);
        
        moveto_special.insert(pair<string, string>(string(moved_special_name), string(full_path_arg)));
        moveto_normal.insert(pair<string, string>(string(full_path_arg), string(moved_special_name)));
    }*/
}

/*void createBackupFileForConflict(char* absolutePath)
{
    char file[BUFLEN];
    strcpy(file, absolutePath);
    int in_fd = open(file, O_RDONLY);
    cout << "Detected conflict for file: " << file << endl;
    strcat(file, ".conflictXXXXXX");
    mkstemp(file);
    inotify_ignore.insert(string(file));
    inotify_ignore_modified.insert(string(file));
    cout << "Old version stored in file: " << file << endl;
    cout << "Please take one of the following actions:" << endl;
    cout << "a) Delete file ending with .conflictXXXXXX to accept version from server." << endl;
    cout << "b) Overwrite orginal file with your changes to force your version. Then delete file ending with .conflictXXXXXX." << endl;
    cout << "Note: next time you connect to the server file ending with .conflictXXXXXX will automatically be deleted." << endl;
    int out_fd = open(file, O_WRONLY);
    char buf[8192];

    while (1)
    {
        ssize_t result = read(in_fd, &buf[0], sizeof(buf));
        if (!result) break;
        write(out_fd, &buf[0], result);
    }
    close(in_fd);
    close(out_fd);
}*/

static void handle_events() {

    char buf[4096];
    const struct inotify_event *event;
    ssize_t len;
    char *ptr;
    char *curr_dir;
    char *path;
    //char pure_path[BUFLEN];
    char full_path[BUFLEN];
    char *buf_ptr = buf;

    /* Loop while events can be read from inotify file descriptor. */
    for(;;) {

        /* Read some events. */
        len = read(in_file_desc, buf, sizeof buf);
        if (len == -1 && errno != EAGAIN) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* If the nonblocking read() found no events to read, then
          it returns -1 with errno set to EAGAIN. In that case,
          we exit the loop. */
        if (len <= 0)
            break;

        /* Loop over all events in the buffer */
        for(ptr = buf; ptr < buf + len;
            ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;
            
            if((event->mask & IN_DELETE) ||
               (event->mask & IN_CLOSE_WRITE) ||
               (event->mask & IN_CREATE) ||
               (event->mask & IN_MOVED_TO) ||
               (event->mask & IN_MOVED_FROM)){
                
                /* find dir where the change happened */
                for(int i=0; i<MAX_DIRS; ++i) {
                    if (watch_desc[i] == event->wd) {
                        curr_dir = dir_list[i];
                        break;
                    }
                }
                
                if(event->len)
                    snprintf(full_path, BUFLEN, "%s/%s", curr_dir, event->name);
                path = full_path + root_length;
            }
            /* ignore all events happening on file which contains list of files */
            if(event->name == SAVED_FILESYSTEM_SUFFIX){
                printf("Ignored an event on %s\n", event->name);
                continue;
            }
            if(event->mask & IN_DELETE) {
                if(event->mask & IN_ISDIR){
                    printf("Inotify detected dir deletion: %s\n", full_path);
                    delete_dir(full_path, path);
                }
                else{
                    printf("Inotify detected file deletion: %s\n", full_path);
                    delete_file(full_path, path);
                }
            }
            if(event->mask & IN_CLOSE_WRITE){
                printf("Inotify detected file modification: %s\n", full_path);
                modify_file(full_path, path);
            }
            if(event->mask & IN_CREATE){
                if(event->mask & IN_ISDIR){
                    printf("Inotify detected dir creation: %s\n", full_path);
                    create_dir(full_path, path);
                }
                else{
                    printf("Inotify detected file creation: %s\n", full_path);
                    create_file(full_path, path);
                }
            } 
            if(event->mask & IN_MOVED_TO){
                if(event->mask & IN_ISDIR)
                    printf("Inotify detected DIRMOVETO %s (cookie=%d)\n", full_path, event->cookie);
                else
                    printf("Inotify detected FILMOVETO %s (cookie=%d)\n", full_path, event->cookie);
                move_to(full_path, path, event);
            } if(event->mask & IN_MOVED_FROM){
            	if(event->mask & IN_ISDIR)
            	    printf("Inofity detected DIRMOVEFR %s\n", full_path);
            	else
            	    printf("Inotify detected FILMOVEFR %s\n", full_path);
                move_from(full_path, path, ptr, event, buf_ptr, len);
            }
        }
    }
}

void receiver_thread_fun(){
    printf("Hello, receiver thread started running!\n");
    char data[BUFLEN];
    char data2[BUFLEN];
    char message_type[BUFLEN];
    char file[BUFLEN];
    char file2[BUFLEN];
    bool exit_thread;
    int backoff_time;
    struct stat helper;

    /*Inform server about initial synch*/
    string root_directory_string = string(root_directory);
    FileSystem *fs = new FileSystem(root_directory_string);
    sendMutex.lock();
    send_string(sock_num, "INITIALSYNCH", "INITIALSYNCH_MSG");
    send_big_string(sock_num, fs->ToString(), "INITIALSYNCH_DATA");
    sendMutex.unlock();
    delete fs;

    /* run until server stopped responding permanently */
    while(1){
        /* run until server has disconnected */
        while(1){
            if(receive_message(sock_num, data) < 0)
                break;
            else {
                strncpy(message_type, data, BUFLEN);
                if(!strcmp(message_type, "DIRCREATE")){
                    if(receive_message(sock_num, data) < 0)
                        break;
                    inotify_ignore.insert(string(data));
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    printf("Receiver: creating directory %s\n", file);
                    if (stat(file, &helper) < 0)
                        mkdir(file, 0755);
                    else
                        printf("Directory already exists!\n");
                }
                else if(!strcmp(message_type, "FILCREATE")){
                    if(receive_message(sock_num, data) < 0)
                        break;

                    time_t lmd = receiveModificationDate(sock_num);
                    if(lmd!=-1)
                        Utility::UpdateLastModificationDate(file, lmd);

                    /* file creation, when received from server causes two inotify events
                       both of which have to be ignored */
                    inotify_ignore.insert(string(data));
                    inotify_ignore_modified.insert(string(data));
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    printf("Receiver: creating file %s\n", file);
                    if (stat(file, &helper) < 0){
                        int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        close(fd);
                    }
                    else
                        printf("File already exists!\n");
                }
                else if(!strcmp(message_type, "FILDELETE")){
                    if(receive_message(sock_num, data) < 0)
                        break;
                    inotify_ignore.insert(string(data));
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    printf("Receiver: deleting file %s\n", file);
                    if (stat(file, &helper) == 0)
                        unlink(file);
                    else
                        printf("File doesn't exist!\n");
                }
                else if(!strcmp(message_type, "DIRDELETE")){
                    if(receive_message(sock_num, data) < 0)
                        break;
                    inotify_ignore.insert(string(data));
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    printf("Receiver: deleting directory %s\n", file);
                    if (stat(file, &helper) == 0)
                        rmdir(file);
                    else
                        printf("Directory doesn't exist!\n");
                }
                else if(!strcmp(message_type, "FILMODIFY")){
                    if(receive_message(sock_num, data) < 0){
                        printf("Server has disconnected during transfer.\n");
                        break;
                    }
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    printf("Receiver: modifying file %s\n", file);

                    time_t lmd = receiveModificationDate(sock_num);
                    if(receive_file(sock_num, file, data, true, &inotify_ignore_modified) < 0){
                        printf("Server has disconnected during data transfer.\n");
                        break;
                    }
                    if(lmd!=-1)
                        Utility::UpdateLastModificationDate(file, lmd);

                    /* this insertion is possibly not required */
                    inotify_ignore_modified.insert(string(data));
                }
                else if(!strcmp(message_type, "MOVED")){
                    if(receive_message(sock_num, data) < 0)
                        break;
                    if(receive_message(sock_num, data2) < 0)
                        break;
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    snprintf(file2, BUFLEN, "%s%s", root_directory, data2);
                    inotify_ignore.insert(data);
                    inotify_ignore.insert(data2);
                    printf("Receiver: moving %s to %s\n", file, file2);
                    if((stat(file, &helper) == -1) || (stat(file2, &helper) == 0))
                        printf("Source file doesn't exist or destination file exists!\n");
                    else
                        rename(file, file2);
                }
                /*else if(!strcmp(message_type, "BIGDELETE")){
                    if(receive_message(sock_num, data) < 0)
                        break;
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    printf("Receiver: deleting %s (file or directory with its contents)\n", file);
                    nftw(file, unlink_callback, 256, FTW_DEPTH | FTW_PHYS);
                }*/
                else if(!strcmp(message_type, "SENDFILE")){
                    if(receive_message(sock_num, data) < 0)
                        break;
                    snprintf(file, BUFLEN, "%s%s", root_directory, data);
                    printf("Receiver: sending file %s\n", file);
                    sendMutex.lock();
                    send_file(sock_num, file, data);
                    sendMutex.unlock();
                }
            }
        }
        printf("Receiver: server has disconnected.\nTrying to reconnect...\n");
        exit_thread = true;
        for(int i=1; i<=MAX_RECONN_ATTEMPTS; i++) {
            if((sock_num = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		        perror("socket");
		        break;
            }
            if(connect(sock_num, (struct sockaddr*) &soc_add, sizeof(soc_add)) < 0) {
                backoff_time = (rand()%10000) * i;
		        printf("Receiver: couldn't connect to server.\nRetrying in %d miliseconds (attempt %d out of %d).\n", backoff_time, i, MAX_RECONN_ATTEMPTS);
            } 
            else {
                printf("Receiver: connected to server.\n");
                exit_thread = false;
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(backoff_time));
        }
        if(exit_thread)
            break;
        else
        {
            sendMutex.lock();
	        send_string(sock_num, "RECONNECTED", "RECONNECTED_MSG");
            sendMutex.unlock();
        }
    }
    printf("Receiver: goodbye!\n");
    if(exit_thread)
        exit(EXIT_FAILURE);
}

void signal_handler(int s){
    printf("Caught signal %d\n", s);
    for(int i=0; i<MAX_DIRS; i++)
        delete [] dir_list[i];
    delete [] dir_list;
    delete [] watch_desc;
    close(in_file_desc);
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {

    srand(time(NULL));
    setbuf(stdout, NULL);

    /* validate arguments */
    if (argc < 4) {
        printf("Usage: %s PATH IP PORT\n", argv[0]);
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

    for(int i=0; i<MAX_RENAMED_FILES; i++)
        renamed_files[i].cookie=-1;

    /* Create the file descriptor for accessing the inotify API */
    in_file_desc = inotify_init1(IN_NONBLOCK);
    if (in_file_desc == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    strncpy(root_directory, argv[1], BUFLEN);
    
    /* remove last slash from root directory */
    for(int l = 0; ; l++) {
        if(l == BUFLEN-1){
            printf("Warning: possible buffer overflow.\n");
            break;
        }
        if(root_directory[l] == 0) {
            if(root_directory[l-1] == '/')
                root_directory[l-1] = 0;
            else
                root_directory[l] = 0;
            break;
        }
    }
    printf("Client directory is %s\n", root_directory);
    
    /* connect to server */
	soc_add.sin_family = AF_INET;
	soc_add.sin_addr.s_addr = inet_addr(argv[2]);
	soc_add.sin_port = htons(port_num);
	if((sock_num = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
    }
    if(connect(sock_num, (struct sockaddr*) &soc_add, sizeof(soc_add)) < 0) {
		perror("connect");
		exit(EXIT_FAILURE);
    }
    
    sendMutex.lock();
    send_string(sock_num, "NORMALCONNECTION", "NORMALCONNECTION_MSG");
    sendMutex.unlock();

    /* start receiver thread */
    thread receiver_th(receiver_thread_fun);

    /* initalize data structures for inotify watches */
    watch_desc = new int[MAX_DIRS];
    dir_list = new char*[MAX_DIRS];
    for(int i=0; i<MAX_DIRS; i++){
        dir_list[i] = new char[BUFLEN];
        dir_list_state[i] = false;
    }
    add_watches_to_subdirs(argv[1]);
    
    /* catch Ctrl+C as program termination */
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    /* Inotify input */
    struct pollfd fds;
    fds.fd = in_file_desc;
    fds.events = POLLIN;

    /* Wait for inotify events */
    int poll_num;
    printf("Listening for events.\nTo stop the program press Ctrl+C\n");
    while (1) {
        poll_num = poll(&fds, 1, -1);
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            exit(EXIT_FAILURE);
        }
        if (poll_num > 0) {
            if (fds.revents & POLLIN) {
                handle_events();
            }
        } 
    }
}
