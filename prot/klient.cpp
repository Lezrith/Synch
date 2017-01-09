/*
KNOWN ISSUES:
 - segfaults when buffer length increased to e.g. 4096 or 8192
 - handling the situation when many dirs are MOVED_TO instantly is not perfect
   and may generate errors and data inconsistency, especially at server's side
*/

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <time.h>
#include <string>
#include <map>
#include <iostream>

#define MAX_DIRS 4096
#define MAX_RENAMED_FILES 16
#define BUFLEN 2048

/*
  BIGDIR tries to fix problems with inotify: when whole directory tree is created instantly,
  inotify can't create watches for all subdirectories that fast, considering example directory tree:
  a/b/
  a/c/
  a/d/
  a/c/e
  a/d/f
  while watch for a/b/ is being created, all other files are written to disk, 
  but there wasn't enough time to create watches for a/c/ and a/d/, 
  so the creation of files inside them isn't detected
  the solution is to tar whole directory a/, send it to server and servers untars it, 
  after the tar is transferred, all watches inside a/ are created manually
  various problems with inotify are described here: https://lwn.net/Articles/605128/
  to see this problem just compile this program without BIGDIR defined and 
  copy into any of watched directories a directory ('bigdir') with many subdirs
  and many files (many >= 100) then compare the number of files
  in original directory and in the (incompletely transferred) 'bigdir' on server
*/

#define BIGDIR

const char *BIGDIR_SPECIAL_NAME = "special_name.tar";
const char *SPECIAL_NAME_SUFFIX = "SPECIAL";
const char *FIND_PATH = "/usr/bin/find";

using namespace std;

struct renamed_file_struct {
    int cookie;
    char old_name[BUFLEN];
} renamed_files[MAX_RENAMED_FILES];
int sock_num;
int root_length;
char **dir_list;
bool dir_list_state[MAX_DIRS];
map<string, string> moveto_special;
map<string, string> moveto_normal;
map<string, string> tar_names;

#ifdef BIGDIR
    char bigdir[BUFLEN];
#endif

/*points to an index where new watch can be created*/
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
/*end of find_free_index*/

/*finds all subdirectories of dir and adds watches for them*/
void add_watches_to_subdirs(char *dir, int *watch_desc, int in_file_desc){
    char find_command[BUFLEN];
    FILE *fp;
    int index;
    snprintf(find_command, BUFLEN, "%s \"%s\" -type d", FIND_PATH, dir);

    fp = popen(find_command, "r");
    if(fp == NULL) {
        perror("popen");
    }
    
    /*index is needed in while loop condition*/
    index = find_free_index();

    while (fgets(dir_list[index], BUFLEN, fp) != NULL) {
        for(int l=0;;l++) {
            if(l == BUFLEN-1){
                printf("Warning: possible buffer overflow.\n");
                continue;
            }
            if(dir_list[index][l] == '\n') {
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
        
        /*add a watch to this dir*/
        watch_desc[index] = inotify_add_watch(in_file_desc, dir_list[index], 
            IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        dir_list_state[index] = true;
        if (watch_desc[index] == -1) {
            fprintf(stderr, "Cannot watch '%s'\n", dir_list[index]);
            perror("inotify_add_watch");
            exit(EXIT_FAILURE);
        }
        
        printf("Added watch %d to %s\n",index,dir_list[index]);
        index = find_free_index();
    }
    pclose(fp);
}
/*end of add_watches_to_subdirs*/

int written;
char arr[BUFLEN];

void send_ch_arr(int sock_num_arg, const char *arr, string msg_name){
    written = send(sock_num_arg, arr, BUFLEN, 0);
    printf("%s: written = %d\n", msg_name.c_str(), written);
}

void send_ch_arr_size(int sock_num_arg, const char *arr, string msg_name, int size){
    written = send(sock_num_arg, arr, size, 0);
    printf("%s: written = %d\n", msg_name.c_str(), written);
}

/*string_to_send can have at most BUFLEN bytes, if it is longer, it'll be truncated*/
void send_string(int sock_num_arg, string string_to_send, string msg_name){
    strncpy(arr, string_to_send.c_str(), BUFLEN);
    written = send(sock_num_arg, arr, BUFLEN, 0);
    printf("%s: written = %d\n", msg_name.c_str(), written);
}

void delete_dir(char *full_path_arg, char *path_arg){

    /*mark the place of old watch as free */
    for(int k=0; k<MAX_DIRS; k++)
        if(!strcmp(full_path_arg, dir_list[k]))
            dir_list_state[k] = false;
         
    /* (1) after the normal MOVED_TO directory was deleted,
    rename special-named temporary directory to original name*/
    map<string, string>::iterator it;
    it = moveto_normal.find(string(full_path_arg));
    if(it != moveto_normal.end()){
        char command[BUFLEN];
        printf("moving %s to %s\n", it->second.c_str(), it->first.c_str());
        snprintf(command, BUFLEN, "mv \"%s\" \"%s\"", it->second.c_str(), it->first.c_str());
        system(command);
        moveto_normal.erase(it);
    }
    
    /*server doesn't have the MOVED_TO dir, send him message only if it isn't such a dir*/
    else {
        printf("DIRDELETE %s\n", full_path_arg);
        send_string(sock_num, "DIRDELETE", "DIRDELETE_MSG");
        send_ch_arr(sock_num, path_arg, "DIRDELETE_DATA");
    }
}
/*end of delete_dir*/

void delete_file(char *full_path_arg, char *path_arg){
    printf("FILDELETE %s\n", full_path_arg);
    send_string(sock_num, "FILDELETE", "FILDELETE_MSG");
    send_ch_arr(sock_num, path_arg, "FILDELETE_DATA");
}
/*end of delete_file*/

void send_file(int sock_num, char *full_path_arg, char *path_arg, char *pure_path_arg, const char *event_name_arg){

    /*TODO the file should possibly be locked in order to prevent data corruption*/
    /*compute how many chunks will it take to transfer the file*/
    FILE *fp=fopen(full_path_arg, "r");
    int filesize; /*max. size = 2 GiB*/
    int chunks;
    if(fp == NULL){
        perror("fopen");
        return;
    }
    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);
    if(filesize < 0)
        printf("Warning: integer overflow.\n");
    fclose(fp);
    chunks = 1 + filesize/BUFLEN;
    
    
    /*transfer the file*/
    int fd = open(full_path_arg,O_RDONLY);
    if(fd < 0) perror("open");
    char chunk_arr[BUFLEN];
    char databuf[BUFLEN];
    int read_succ;
    printf("file_size=%d chunks=%d\n", filesize, chunks);
    
    /*THE FOLLOWING CODE SHOULD BE REMOVED IF USING THIS PROCEDURE OUTSIDE THIS FILE ...*/
#ifdef BIGDIR
    map<string, string>::iterator it;
    it = tar_names.find(string(event_name_arg));
    if(it != tar_names.end()) {
        printf("Handling tar %s\n", it->first.c_str());
        send_string(sock_num, "BIGDIR", "BIGDIR_MSG");
        printf("pure_path_arg = %s\n", pure_path_arg);
        send_ch_arr(sock_num, pure_path_arg, "BIGDIR_PATH");
        
        /*don't send tar's path, beacuse server when untars it changes
        directory anyway so sending whole path would generate error*/
        send_string(sock_num, it->first, "BIGDIR_NAME");
    }
    else {
        send_string(sock_num, "FILMODIFY", "FILMODIFY_MSG");
        send_ch_arr(sock_num, path_arg, "FILMODIFY_NAME");
    }
#endif
    /*... UNTIL HERE, THEN REMOVE TWO COMPILER DIRECTIVES BELOW*/
#ifndef BIGDIR
    send_string(sock_num, "FILMODIFY", "FILMODIFY_MSG");
    send_ch_arr(sock_num, path_arg, "FILMODIFY_NAME");
#endif
    snprintf(chunk_arr, BUFLEN, "%d", chunks);
    send_ch_arr(sock_num, chunk_arr, "FILMODIFY_CHUNKS");
    for(int i=1; i<=chunks; i++){
        read_succ = read(fd, databuf, BUFLEN);
        snprintf(chunk_arr, BUFLEN, "%d", read_succ);
        send_ch_arr(sock_num, chunk_arr, "FILMODIFY_CHUNK_SIZE");
        snprintf(chunk_arr, BUFLEN, "FILMODIFY_DATA_%d", i);
        if(read_succ < BUFLEN)
            send_ch_arr_size(sock_num, databuf, chunk_arr, read_succ);
        else
            send_ch_arr(sock_num, databuf, chunk_arr);
    }
    close(fd);
}
/*end of send_file*/

void modify_file(char *full_path_arg, const char *event_name_arg, char *pure_path_arg, char *path_arg, int *watch_desc, int in_file_desc){

    map<string, string>::iterator it;
    map<string, string>::iterator it2;
    string helper_string;
    printf("FILMODIFY %s\n", full_path_arg);
    
    send_file(sock_num, full_path_arg, path_arg, pure_path_arg, event_name_arg);

#ifdef BIGDIR
    /*remove temoporary tar and manually add watches for subdirectories of dir
    that was transferred as tar*/
    it = tar_names.find(string(event_name_arg));
    if(it != tar_names.end()){
        char command_buf[BUFLEN];
        printf("Removing temporary tar %s\n", full_path_arg);
        snprintf(command_buf, BUFLEN, "rm \"%s\"", full_path_arg);
        system(command_buf);
        helper_string = it->second.c_str();
        printf("Adding watches for subdirectories of %s\n", helper_string.c_str());
        char arg[BUFLEN];
        strncpy(arg, it->second.c_str(), BUFLEN);
        add_watches_to_subdirs(arg, watch_desc, in_file_desc);
    }
#endif
    
    map<string, string>::iterator current_bigdir;
    printf("Searching for %s in tar_names\n", event_name_arg);
    current_bigdir = tar_names.find(string(event_name_arg));
    
    if(current_bigdir == tar_names.end()){ 
        it = moveto_special.find(string(full_path_arg));
        
        /*this isn't special name of dir, it's just a file which was MOVED_TO
        rename it to original name*/
        if(it != moveto_special.end()){
            char command[BUFLEN];
            printf("Moving %s to %s\n", it->first.c_str(), it->second.c_str());
            snprintf(command, BUFLEN, "mv \"%s\" \"%s\"", it->first.c_str(), it->second.c_str());
            system(command);
            
            /*erase complementary entry*/
            it2 = moveto_normal.find(it->second);
            printf("Searching for %s in moveto_normal\n", it2->second.c_str());
            if(it2 != moveto_normal.end()){
                printf("Erasing %s from moveto_normal\n", it2->first.c_str());
                moveto_normal.erase(it2);
            }
            moveto_special.erase(it);
        }
    } 
    
    /*this is MOVED_TO dir's copy
    remove original MOVED_TO dir so we will be able to rename the copy to original name */
    else {
        tar_names.erase(current_bigdir);
        printf("It's a moved_to dir, searching for %s in moveto_special\n", helper_string.c_str());
        it = moveto_special.find(helper_string);
        if(it != moveto_special.end()){
            char command[BUFLEN];
            printf("Removing %s\n", it->second.c_str());
            snprintf(command, BUFLEN, "rm -r \"%s\"", it->second.c_str());
            system(command);
            moveto_special.erase(it);
            /*GOTO (1)*/
        }
    }
}
/*end of modify_file*/

void create_dir(char *full_path_arg, char *path_arg, char *curr_dir_arg, const char *event_name_arg){

#ifndef BIGDIR
    /*when using BIGDIR, directories (even empty) are tarred and transferred 
    like ordinary files so this code isn't needed*/
    printf("DIRCREATE %s\n", full_path_arg);
    send_string(sock_num, "DIRCREATE", "DIRCREATE_MSG");
    send_ch_arr(sock_num, path_arg, "DIRCREATE_DATA");
    
    /*add watch for new directory*/
    int dir_list_index = find_free_index();
    dir_list_state[dir_list_index] = true;
    
    strncpy(dir_list[dir_list_index], full_path_arg, BUFLEN);
    watch_desc[dir_list_index] = inotify_add_watch(fd, dir_list[dir_list_index],
        IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        if (watch_desc[dir_list_index] == -1) {
            fprintf(stderr, "Cannot watch '%s'\n", dir_list[dir_list_index]);
            perror("inotify_add_watch");
            exit(EXIT_FAILURE);
        }
        
    printf("ADDWATCH (%d) %s\n", dir_list_index, dir_list[dir_list_index]);
    
#endif
#ifdef BIGDIR
       
    /*wait some time until copying of big dir finishes, 
    if dirs are rather big, this time should be long too!*/
    usleep(1000000);

    /*tar the big dir*/
    strncpy(bigdir, curr_dir_arg, BUFLEN);
    char command[BUFLEN];
    char tar_name[BUFLEN];
    snprintf(tar_name, BUFLEN, "%s%s", event_name_arg, BIGDIR_SPECIAL_NAME);
    snprintf(command, BUFLEN, "cd \"%s\" && tar -cf \"%s\" \"%s\"", 
        curr_dir_arg, tar_name, event_name_arg);
    tar_names.insert(pair<string, string>(string(tar_name), string(full_path_arg)));
    printf("Creating tar: command = %s\n", command);
    system(command);
                
#endif
}
/*end of create dir*/

void create_file(char *full_path_arg, char *path_arg){
    printf("FILCREATE %s\n", full_path_arg);
    send_string(sock_num, "FILCREATE", "FILCREATE_MSG");
    send_ch_arr(sock_num, path_arg, "FILCREATE_DATA");
}
/*end of create_file*/

void move_from(char *full_path_arg, char *path_arg, char *ptr_arg, const struct inotify_event *event_arg, char *buf_ptr, ssize_t len_arg){

    char *ptr_temp;
    const struct inotify_event *event_temp;
    int file_number;
    
    /*check if this event has corresponding IN_MOVED_TO in the buffer
    this is ALMOST always the case when renaming files*/
    ptr_temp = ptr_arg;
    event_temp = event_arg;
    bool renaming = false;
    while(ptr_temp < buf_ptr + len_arg){
        ptr_temp += sizeof(struct inotify_event) + event_temp->len;
        event_temp = (const struct inotify_event *) ptr_temp;
        if(event_temp->cookie == event_arg->cookie) {
            printf("Corresponding cookie %d found in the buffer!\n", event_temp->cookie);
            renaming = true;
        }
    }
    
    /*no cookie found, this is probably permanent deletion,
    server will have to deal with it on its own*/
    if(!renaming) {
        printf("No cookie found!\n");
        printf("BIGDELETE %s\n", full_path_arg);
        send_string(sock_num, "BIGDELETE", "BIGDELETE_MSG");
        send_ch_arr(sock_num, path_arg, "BIGDELETE_DATA");
        
        /*mark the place of old watch as free*/
        for(int k=0; k<MAX_DIRS; k++)
            if(!strcmp(full_path_arg, dir_list[k]))
                dir_list_state[k] = false;
    }
    
    else {
        /*find a place to save a cookie and old filename*/
        for(int j=0; j<MAX_RENAMED_FILES; j++)
            if(renamed_files[j].cookie == -1) { 
                file_number = j;
                break;
            }
        renamed_files[file_number].cookie = event_arg->cookie;
        strncpy(renamed_files[file_number].old_name, path_arg, BUFLEN);
    }
}
/*end of move_from*/

void move_to(char *full_path_arg, char *path_arg, const struct inotify_event *event_arg, int *watch_desc, int in_file_desc){
    bool found_corresp_cookie = false;
    
    for(int j=0; j<MAX_RENAMED_FILES; j++) {
    
        /*we found which IN_MOVED_FROM event corresponds to current one*/
        if(event_arg->cookie == (unsigned)renamed_files[j].cookie) {
            renamed_files[j].cookie=-1;
            found_corresp_cookie = true;
            
            send_string(sock_num, "MOVED", "MOVED_MSG");
            send_ch_arr(sock_num, renamed_files[j].old_name, "MOVED_FROM");
            send_ch_arr(sock_num, path_arg, "MOVED_TO");
            
            if(event_arg->mask & IN_ISDIR){ /*deal with outdated watches*/
                char *outdated_dir;
                int result;
                char *dir_list_slash;
                char *old_name_slash;
                dir_list_slash = (char *) calloc(BUFLEN+1, sizeof(char));
                old_name_slash = (char *) calloc(BUFLEN+1, sizeof(char));
                if(!dir_list_slash || !old_name_slash)
                    perror("calloc");
                printf("Dealing with outdated watch: %s\n", renamed_files[j].old_name);
                for(int k=0; k<MAX_DIRS; k++){
                
                    /*if we don't add slash then such situation may happen:
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
                        printf("Removing watch %s\n", outdated_dir);
                        result = inotify_rm_watch(in_file_desc, watch_desc[k]);
                        if(result < 0) perror("inotify_rm_watch");
                        else dir_list_state[k] = false;
                    }
                }
                free(dir_list_slash);
                free(old_name_slash);
                add_watches_to_subdirs(full_path_arg, watch_desc, in_file_desc);
            }
            break;
        }
    }
    
    /*no corresponding cookie found - file or dir was moved into watched directory;
    copy it so it's appearance will be detected and if it is a dir, it'll be
    transferred as tar*/
    if(!found_corresp_cookie){
        char command[BUFLEN];
        char moved_special_name[BUFLEN];
        snprintf(moved_special_name, BUFLEN, "%s%s", full_path_arg, SPECIAL_NAME_SUFFIX);
        printf("copying %s to %s\n", full_path_arg, moved_special_name);
        snprintf(command, BUFLEN, "cp -r \"%s\" \"%s\"", full_path_arg, moved_special_name);
        system(command);
        
        moveto_special.insert(pair<string, string>(string(moved_special_name), string(full_path_arg)));
        moveto_normal.insert(pair<string, string>(string(full_path_arg), string(moved_special_name)));
    }
}
/*end of move_to*/

static void handle_events(int in_file_desc, int *watch_desc) {

    char buf[4096];
    const struct inotify_event *event;
    ssize_t len;
    char *ptr;
    char *curr_dir;
    char path[BUFLEN];
    char pure_path[BUFLEN];
    char full_path[BUFLEN];
    char *buf_ptr = buf;


    /* Loop while events can be read from inotify file descriptor. */

    for (;;) {

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

        for (ptr = buf; ptr < buf + len;
            ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;

            for(int a=0; a<BUFLEN; a++){
                path[a] = 0;
                pure_path[a] = 0;
            }
            
            if((event->mask & IN_DELETE) ||
               (event->mask & IN_CLOSE_WRITE) ||
               (event->mask & IN_CREATE) ||
               (event->mask & IN_MOVED_TO) ||
               (event->mask & IN_MOVED_FROM)){
               
                /*controlling if maps are emptied*/
                printf("================== %lu %lu %lu =================\n", 
                    moveto_special.size(), moveto_normal.size(), tar_names.size());
                
                /*find dir where the change happened*/
                for (int i=0; i<MAX_DIRS; ++i) {
                    if (watch_desc[i] == event->wd) {
                        curr_dir = dir_list[i];
                        break;
                    }
                }
                
                /*possible redundancy in code determining pure_path and path*/
                /*extract only relative directory path of event (without filename at the end)*/
                int j = 0;
                for(int i=0; i<BUFLEN; i++){
                    if(curr_dir[i] == dir_list[0][i])
                        continue;
                    pure_path[j] = curr_dir[i+1];
                    j++;
                }
                
                if (event->len){
                    snprintf(full_path, BUFLEN, "%s/%s", curr_dir, event->name);
                    
                    /*get relative path by removing parent directories from program's root dir*/
                    int x, y;
                    for(x=root_length, y=0; full_path[x]!=0; x++, y++)
                        path[y] = full_path[x];
                }
                printf("pure_path = %s\npath=%s\n",pure_path,path);
            }

            if (event->mask & IN_DELETE) {
                if(event->mask & IN_ISDIR)
                    delete_dir(full_path, path);
                else
                    delete_file(full_path, path);
            }
            
            if (event->mask & IN_CLOSE_WRITE)
                modify_file(full_path, event->name, pure_path, path, watch_desc, in_file_desc);
            
            if (event->mask & IN_CREATE) {
                if(event->mask & IN_ISDIR)
                    create_dir(full_path, path, curr_dir, event->name);
                else
                    create_file(full_path, path);
            } 
            
            if(event->mask & IN_MOVED_TO){
                if(event->mask & IN_ISDIR)
                    printf("DIRMOVETO %s (cookie=%d)\n", full_path, event->cookie);
                else
                    printf("FILMOVETO %s (cookie=%d)\n", full_path, event->cookie);
                move_to(full_path, path, event, watch_desc, in_file_desc);
            } if(event->mask & IN_MOVED_FROM){
            	if(event->mask & IN_ISDIR)
            	    printf("DIRMOVEFR %s\n", full_path);
            	else
            	    printf("FILMOVEFR %s\n", full_path);
                move_from(full_path, path, ptr, event, buf_ptr, len);
            }
        }
    }
}
/*end of handle_events*/

int main(int argc, char* argv[]) {
    nfds_t nfds;
    struct pollfd fds[2];
    int in_file_desc;

    setbuf(stdout, NULL);

    for(int i=0; i<MAX_RENAMED_FILES; i++)
        renamed_files[i].cookie=-1;
    
    if (argc < 4) {
        printf("Usage: %s PATH IP PORT\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Press ENTER key to terminate.\n");

    /* Create the file descriptor for accessing the inotify API */
    in_file_desc = inotify_init1(IN_NONBLOCK);
    if (in_file_desc == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }
    
    /*connect ot server*/
    struct sockaddr_in soc_add;
	soc_add.sin_family = AF_INET;
	soc_add.sin_addr.s_addr = inet_addr(argv[2]);
	soc_add.sin_port = htons(atoi(argv[3]));
	if((sock_num = socket(AF_INET, SOCK_STREAM,0)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
    }
	if(connect(sock_num, (struct sockaddr*) &soc_add, sizeof(soc_add)) < 0) {
		perror("connect");
		exit(EXIT_FAILURE);
    }

    int *watch_desc;
    watch_desc = new int[MAX_DIRS];

    dir_list = new char*[MAX_DIRS];
    for(int i=0; i<MAX_DIRS; i++){
        dir_list[i] = new char[BUFLEN];
        dir_list_state[i] = false;
    }
        
    add_watches_to_subdirs(argv[1], watch_desc, in_file_desc);

    nfds = 2;

    /* Console input */
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    /* Inotify input */
    fds[1].fd = in_file_desc;
    fds[1].events = POLLIN;

    /* Wait for events and/or terminal input */
    char buf;
    int poll_num;
    printf("Listening for events.\n");
    while (1) {
        poll_num = poll(fds, nfds, -1);
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if (poll_num > 0) {
            if (fds[0].revents & POLLIN) {
                /* Console input is available. Empty stdin and quit */
                while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
                    continue;
                break;
            }
            
            if (fds[1].revents & POLLIN) {
                /* Inotify events are available */
                handle_events(in_file_desc, watch_desc);
            }
        } 
    }
    
    printf("Listening for events stopped.\n");

    /* Close inotify file descriptor */
    for(int i=0; i<MAX_DIRS; i++)
        delete [] dir_list[i];
    delete [] dir_list;
    delete [] watch_desc;
    close(in_file_desc);
    exit(EXIT_SUCCESS);
}
