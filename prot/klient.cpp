#include <errno.h>
#include <poll.h>
#include <assert.h>
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
#include <map>
#include <iostream>
#include <ftw.h>

#define MAX_DIRS 4096
#define MAX_RENAMED_FILES 32
#define BUFLEN 2048
#define SMALLBUF 16
#define FLAGS IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO

using namespace std;

struct renamed_file_struct {
    int cookie;
    char old_name[BUFLEN];
} renamed_files[MAX_RENAMED_FILES];
int sock_num;
int root_length;
char **dir_list;
bool dir_list_state[MAX_DIRS];
char databuf[BUFLEN];
map<string, string> moveto_special;
map<string, string> moveto_normal;

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

int *global_watch_desc;
int global_in_file_desc;

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
    global_watch_desc[index] = inotify_add_watch(global_in_file_desc, dir_list[index], FLAGS);
    dir_list_state[index] = true;
    if (global_watch_desc[index] == -1) {
        fprintf(stderr, "Cannot watch '%s'\n", dir_list[index]);
        perror("inotify_add_watch");
        exit(EXIT_FAILURE);
    }
    
    printf("Added watch %d to %s\n", index, dir_list[index]);
    return 0;
}

/* finds all subdirectories of dir and adds watches for them */
void add_watches_to_subdirs(char *dir, int *watch_desc, int in_file_desc){

    int result;
    global_watch_desc = watch_desc;
    global_in_file_desc = in_file_desc;
    result = nftw(dir, add_wat, 0, 0);
    if(result < 0)
        perror("nftw");

}

/* sends arr to sock_num_arg, prints debug information using msg_name 
   if desired_length equals 0, length of array is checked, otherwise desired_length bytes is sent */
void send_ch_arr(int sock_num_arg, const char *arr, string msg_name, int desired_length){
    int written;
    /* TODO client should try to reconect to server */
    /* check the size of array */
    int arr_len;
    if(desired_length == 0)
        for(arr_len = 0; arr[arr_len] != 0; arr_len++);
    else
        arr_len = desired_length;

    int *ptr = &arr_len;
    assert(arr_len <= BUFLEN);
    
    /* send number of bytes to be expected */
    written = send(sock_num_arg, ptr, sizeof(int), 0);
    printf("%s: packet_size   written = %d \n", msg_name.c_str(), written);
    if(written < 0)
        exit(EXIT_FAILURE);
    
    /* send actual data */
    written = send(sock_num_arg, arr, arr_len, 0);
    printf("%s: actual_packet written = %d \n", msg_name.c_str(), written);
    if(written < 0)
        exit(EXIT_FAILURE);
}

/* string_to_send can have at most BUFLEN bytes */
void send_string(int sock_num_arg, string string_to_send, string msg_name){
    send_ch_arr(sock_num_arg, string_to_send.c_str(), msg_name, 0);
}

void delete_dir(char *full_path_arg, char *path_arg){

    /* mark the place of old watch as free */
    for(int k=0; k<MAX_DIRS; k++)
        if(!strcmp(full_path_arg, dir_list[k]))
            dir_list_state[k] = false;

    printf("DIRDELETE %s\n", full_path_arg);
    send_string(sock_num, "DIRDELETE", "DIRDELETE_MSG");
    send_ch_arr(sock_num, path_arg, "DIRDELETE_DATA", 0);
}

void delete_file(char *full_path_arg, char *path_arg){
    printf("FILDELETE %s\n", full_path_arg);
    send_string(sock_num, "FILDELETE", "FILDELETE_MSG");
    send_ch_arr(sock_num, path_arg, "FILDELETE_DATA", 0);
}

void send_file(int sock_num, char *full_path_arg, char *path_arg){

    /* check file size and number of chunks to send */
    int chunks;
    struct stat tr_file;
    if (stat(full_path_arg, &tr_file) == -1) {
        char err_msg[BUFLEN];
        snprintf(err_msg, BUFLEN, "Error while sending file %s: stat", full_path_arg);
        perror(err_msg);
        exit(EXIT_FAILURE);
    }
    long long filesize;
    filesize = tr_file.st_size;
    chunks = ceil((float)filesize/(float)BUFLEN);
    
    /* transfer the file */
    int fd = open(full_path_arg, O_RDONLY);
    if(fd < 0){
        char err_msg[BUFLEN];
        snprintf(err_msg, BUFLEN, "Error while opening %s", full_path_arg);
        perror(err_msg);
        exit(EXIT_FAILURE);
    }
    char chunk_arr[SMALLBUF];
    int read_succ;
    printf("file_size=%lld chunks=%d\n", filesize, chunks);
    
    send_string(sock_num, "FILMODIFY", "FILMODIFY_MSG");
    send_ch_arr(sock_num, path_arg, "FILMODIFY_NAME", 0);
    snprintf(chunk_arr, SMALLBUF, "%d;", chunks);
    string number_str = string(chunk_arr);
    size_t number_len = number_str.find(";");
    number_str.erase(number_len, string::npos);
    printf("chunks = %d Gonna send: %s\n", chunks, number_str.c_str());
    send_string(sock_num, number_str, "FILMODIFY_CHUNKS");
    for(int i=1; i<=chunks; i++){
        read_succ = read(fd, databuf, BUFLEN);
        snprintf(chunk_arr, SMALLBUF, "FILMODIFY_DATA_%d", i);
        int minim = min(read_succ, BUFLEN);
        send_ch_arr(sock_num, databuf, chunk_arr, minim);
    }
    close(fd);
}

void modify_file(char *full_path_arg, char *path_arg){

    map<string, string>::iterator it;
    map<string, string>::iterator it2;
    string helper_string;
    printf("FILMODIFY %s\n", full_path_arg);
    
    send_file(sock_num, full_path_arg, path_arg);
    
    it = moveto_special.find(string(full_path_arg));
    
    /* this is a file which was MOVED_TO, rename it to original name */
    if(it != moveto_special.end()){

        printf("Moving %s to %s\n", it->first.c_str(), it->second.c_str());
        rename(it->first.c_str(), it->second.c_str());
        
        /* erase complementary entry */
        it2 = moveto_normal.find(it->second);
        printf("Searching for %s in moveto_normal\n", it2->second.c_str());
        if(it2 != moveto_normal.end()){
            printf("Erasing %s from moveto_normal\n", it2->first.c_str());
            moveto_normal.erase(it2);
        }
        moveto_special.erase(it);
    }
}

void create_dir(char *full_path_arg, char *path_arg, int in_file_desc, int *watch_desc){

    printf("DIRCREATE %s\n", full_path_arg);
    send_string(sock_num, "DIRCREATE", "DIRCREATE_MSG");
    send_ch_arr(sock_num, path_arg, "DIRCREATE_DATA", 0);
    
    /* add watch for new directory */
    int dir_list_index = find_free_index();
    dir_list_state[dir_list_index] = true;
    
    strncpy(dir_list[dir_list_index], full_path_arg, BUFLEN);
    watch_desc[dir_list_index] = inotify_add_watch(in_file_desc, dir_list[dir_list_index], FLAGS);
        if (watch_desc[dir_list_index] == -1) {
            fprintf(stderr, "Cannot watch '%s'\n", dir_list[dir_list_index]);
            perror("inotify_add_watch");
            exit(EXIT_FAILURE);
        }
        
    printf("ADDWATCH (%d) %s\n", dir_list_index, dir_list[dir_list_index]);
}

void create_file(char *full_path_arg, char *path_arg){
    printf("FILCREATE %s\n", full_path_arg);
    send_string(sock_num, "FILCREATE", "FILCREATE_MSG");
    send_ch_arr(sock_num, path_arg, "FILCREATE_DATA", 0);
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
    bool renaming = false;
    while(ptr_temp < buf_ptr + len_arg){
        ptr_temp += sizeof(struct inotify_event) + event_temp->len;
        event_temp = (const struct inotify_event *) ptr_temp;
        if(event_temp->cookie == event_arg->cookie) {
            printf("Corresponding cookie %d found in the buffer!\n", event_temp->cookie);
            renaming = true;
        }
    }
    
    /* no cookie found, this is probably permanent deletion,
    server will have to deal with it on its own */
    if(!renaming) {
        printf("No cookie found!\n");
        printf("BIGDELETE %s\n", full_path_arg);
        send_string(sock_num, "BIGDELETE", "BIGDELETE_MSG");
        send_ch_arr(sock_num, path_arg, "BIGDELETE_DATA", 0);
        
        /* mark the place of old watch as free */
        for(int k=0; k<MAX_DIRS; k++)
            if(!strcmp(full_path_arg, dir_list[k]))
                dir_list_state[k] = false;
    }
    
    else {
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
    }
}

void move_to(char *full_path_arg, char *path_arg,
             const struct inotify_event *event_arg, int *watch_desc, int in_file_desc){
    bool found_corresp_cookie = false;
    
    for(int j=0; j<MAX_RENAMED_FILES; j++) {
    
        /* we found which IN_MOVED_FROM event corresponds to current one */
        if(event_arg->cookie == (unsigned)renamed_files[j].cookie) {
            renamed_files[j].cookie=-1;
            found_corresp_cookie = true;
            
            send_string(sock_num, "MOVED", "MOVED_MSG");
            send_ch_arr(sock_num, renamed_files[j].old_name, "MOVED_FROM", 0);
            send_ch_arr(sock_num, path_arg, "MOVED_TO", 0);
            
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
    
    /* no corresponding cookie found - file was moved into watched directory;
    copy it so it's appearance will be detected */
    if(!found_corresp_cookie){
        char moved_special_name[BUFLEN];
        
        /* get the name for temporary file */
        snprintf(moved_special_name, BUFLEN, "%sXXXXXX", full_path_arg);
        int fd_out = mkstemp(moved_special_name);
        if(fd_out < 0)
            perror("mkstemp");
        
        int fd_in = open(full_path_arg, O_RDONLY);
        if(fd_in < 0){
            char err_msg[BUFLEN];
            snprintf(err_msg, BUFLEN, "Error while opening %s", full_path_arg);
            perror(err_msg);
            exit(EXIT_FAILURE);
        }

        /* copy MOVED_TO file to a temporary file */
        char buffer[16384];
        int read_from_input;
        while((read_from_input = read(fd_in, buffer, 16384)) > 0)
            write(fd_out, buffer, read_from_input);
        
        close(fd_out);
        close(fd_in);
        
        moveto_special.insert(pair<string, string>(string(moved_special_name), string(full_path_arg)));
        moveto_normal.insert(pair<string, string>(string(full_path_arg), string(moved_special_name)));
    }
}

static void handle_events(int in_file_desc, int *watch_desc) {

    char buf[4096];
    const struct inotify_event *event;
    ssize_t len;
    char *ptr;
    char *curr_dir;
    char *path;
    char pure_path[BUFLEN];
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

            for(int a=0; a<BUFLEN; a++)
                pure_path[a] = 0;
            
            if((event->mask & IN_DELETE) ||
               (event->mask & IN_CLOSE_WRITE) ||
               (event->mask & IN_CREATE) ||
               (event->mask & IN_MOVED_TO) ||
               (event->mask & IN_MOVED_FROM)){
               
                /* controlling if maps are emptied */
                printf("================== %lu %lu =================\n", 
                    moveto_special.size(), moveto_normal.size());
                
                /* find dir where the change happened */
                for(int i=0; i<MAX_DIRS; ++i) {
                    if (watch_desc[i] == event->wd) {
                        curr_dir = dir_list[i];
                        break;
                    }
                }
                
                /* extract only relative directory path of event (without filename at the end) */
                int j = 0;
                for(int i=0; i<BUFLEN; i++){
                    if(curr_dir[i] == dir_list[0][i])
                        continue;
                    pure_path[j] = curr_dir[i+1];
                    j++;
                }
                
                if(event->len)
                    snprintf(full_path, BUFLEN, "%s/%s", curr_dir, event->name);
                path = full_path + root_length;
                printf("pure_path = %s\npath=%s\n", pure_path, path);
            }

            if(event->mask & IN_DELETE) {
                if(event->mask & IN_ISDIR)
                    delete_dir(full_path, path);
                else
                    delete_file(full_path, path);
            }
            
            if(event->mask & IN_CLOSE_WRITE)
                modify_file(full_path, path);
            
            if(event->mask & IN_CREATE) {
                if(event->mask & IN_ISDIR)
                    create_dir(full_path, path, in_file_desc, watch_desc);
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
/* end of handle_events */

int main(int argc, char* argv[]) {

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

    setbuf(stdout, NULL);

    for(int i=0; i<MAX_RENAMED_FILES; i++)
        renamed_files[i].cookie=-1;

    printf("Press ENTER key to terminate.\n");

    /* Create the file descriptor for accessing the inotify API */
    int in_file_desc;
    in_file_desc = inotify_init1(IN_NONBLOCK);
    if (in_file_desc == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }
    
    /* connect to server */
    struct sockaddr_in soc_add;
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

    int *watch_desc;
    watch_desc = new int[MAX_DIRS];

    dir_list = new char*[MAX_DIRS];
    for(int i=0; i<MAX_DIRS; i++){
        dir_list[i] = new char[BUFLEN];
        dir_list_state[i] = false;
    }
        
    add_watches_to_subdirs(argv[1], watch_desc, in_file_desc);

    nfds_t nfds;
    nfds = 2;
    struct pollfd fds[2];

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
