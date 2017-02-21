#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <set>
#include <string>
#include <sys/socket.h>
#include <assert.h>
#include <sys/stat.h>
#include <ftw.h>
#include <math.h>
#include "Network.h"

using namespace std;

int receive_message(int sock_arg, char *data_arg){
    unsigned to_be_received;
    unsigned *ptr = &to_be_received;  
    int received;
    unsigned total_received = 0;
    while(total_received < sizeof(int)){
        received = recv(sock_arg, ptr + total_received*sizeof(char), sizeof(int) - total_received, 0);
        if(received <= 0)
            return -1;
        total_received += received;
    }
    //printf("received = %d bytes = %d\n", received, to_be_received);
    for(int i=0; i<BUFLEN; i++)
        data_arg[i] = 0;
    total_received = 0;
    while(total_received < to_be_received){
        received = recv(sock_arg, data_arg + total_received*sizeof(char), to_be_received - total_received, 0);
        if(received <= 0)
            return -2;
        //printf("received_data = %d\n", received);
        total_received += received;
    }
    return total_received;
}

/* if client_side is true, third and fifth argument must be valid pointers,
   otherwise these arguments must be set to NULL */
int receive_file(int sock_arg, char *file_path_arg, char *path_arg, bool client_side, set<string> *inotify_ignore_modified){

    if(client_side){
        assert(inotify_ignore_modified != NULL);
        assert(path_arg != NULL);
    } else {
        assert(inotify_ignore_modified == NULL);
        assert(path_arg == NULL);
    }

    char temp_data[BUFLEN];
    /* receive how many chunks are there going to be sent */
    receive_message(sock_arg, temp_data);
    int chunks = atoi(temp_data);
    printf("There will be %d chunks.\n", chunks);
    int bytes;

    /* write to file which is modified */
    if(chunks>0){
        int fd=open(file_path_arg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd<0){
            perror("open");
            return -1;
        }

        for(int i=1; i<=chunks; i++){
            printf("Receiving chunk %d out of %d.\n", i, chunks);
            if((bytes = receive_message(sock_arg, temp_data)) < 0){
                printf("Connection lost during transfer.\nData in file %s is likely to be corrupted.\n", file_path_arg);
                close(fd);
                return -1;
            }
            write(fd, temp_data, bytes);
        }

        if(client_side){
            /* receiving non-empty file from server causes modification event,
               which we want to ignore */
            inotify_ignore_modified->insert(string(path_arg));
        }
        close(fd);
    }
    return 0;
}

/* sends arr to sock_num_arg, prints debug information using msg_name 
   if desired_length equals 0, length of array is checked, otherwise desired_length bytes is sent */
/* returns -1 if client disconnected during transfer */
int send_ch_arr(int sock_num_arg, const char *arr, string msg_name, int desired_length){
    int written;

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
    //printf("%s: packet_size   written = %d \n", msg_name.c_str(), written);
    if(written < (signed)sizeof(int))
       return -1;
    
    /* send actual data */
    written = send(sock_num_arg, arr, arr_len, 0);
    //printf("%s: actual_packet written = %d \n", msg_name.c_str(), written);
    if(written < arr_len)
        return -1;
    return 0;
}

/* string_to_send can have at most BUFLEN bytes */
void send_string(int sock_num_arg, string string_to_send, string msg_name){
    send_ch_arr(sock_num_arg, string_to_send.c_str(), msg_name, 0);
}

int send_file(int sock_num, const char *full_path_arg, char *path_arg){

    char databuf[BUFLEN];
    int status;
    /* check file size and number of chunks to send */
    int chunks;
    struct stat tr_file;
    if (stat(full_path_arg, &tr_file) == -1) {
        char err_msg[BUFLEN];
        snprintf(err_msg, BUFLEN, "Error while sending file %s: stat", full_path_arg);
        perror(err_msg);
        return -1;
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
        return -1;
    }
    char chunk_arr[SMALLBUF];
    int read_succ;
    printf("File size is %lld bytes.\n", filesize);
    
    send_string(sock_num, "FILMODIFY", "FILMODIFY_MSG");
    send_ch_arr(sock_num, path_arg, "FILMODIFY_NAME", 0);
    sendLastModificationDate(sock_num, full_path_arg);
    snprintf(chunk_arr, SMALLBUF, "%d;", chunks);
    string number_str = string(chunk_arr);
    size_t number_len = number_str.find(";");
    number_str.erase(number_len, string::npos);
    send_string(sock_num, number_str, "FILMODIFY_CHUNKS");
    for(int i=1; i<=chunks; i++){
        printf("Sending chunk %d out of %d.\n", i, chunks);
        read_succ = read(fd, databuf, BUFLEN);
        snprintf(chunk_arr, SMALLBUF, "FILMODIFY_DATA_%d", i);
        int minim = min(read_succ, BUFLEN);
        status = send_ch_arr(sock_num, databuf, chunk_arr, minim);
        if(status == -1){
            printf("Sending failed.\n");
            break;
        }
    }
    close(fd);
    if(status == -1)
        return -1;
    return 0;
}

void sendLastModificationDate(int clientSocket, string absolutePath)
{
    time_t lmd = Utility::GetLastModificationDate(absolutePath);
    send_string(clientSocket, "LASTMODDATE", "LASTMODDATE_MSG");
    send_string(clientSocket, to_string(lmd), "LATMODDATE_DATA");
}

time_t receiveModificationDate(int clientSocket)
{
    char data[SMALLBUF];
    receive_message(clientSocket, data);
    char data2[BUFLEN];
    if(!strcmp(data, "LASTMODDATE"))
    {
        time_t time;
        receive_message(clientSocket, data2);
        try
        {
            time = stoi(string(data2));
            return time;
        } catch(const invalid_argument &e)
        {
            cout << "Error receiving last modification date from client: " << clientSocket << endl;
            cout << "data2=" << data2 << endl;
        }
    }
    else
    {
        cout << "Error receiving last modification date from client: " << clientSocket << endl;
        cout << "data=" << data << endl;
    }
    return -1;
}

void send_big_string(int sock_num_arg, string string_to_send, string msg_name)
{
    int length = string_to_send.size();
    int chunks = ceil((float)length/(float)BUFLEN);
    send_string(sock_num_arg, to_string(chunks), "BIGSTRING_CHUNKS");
    for(int i=0; i<length; i+=BUFLEN)
        send_string(sock_num_arg, string_to_send.substr(i, BUFLEN), "BIGSTRING_DATA");
}

string receive_big_string(int sock_arg)
{
    char temp_data[BUFLEN];
    /* check file size and number of chunks to send */
    receive_message(sock_arg, temp_data);
    string result="";
    int bytes;
    int chunks;
    try {
        chunks = stoi(temp_data);
        for (int i = 1; i <= chunks; i++) {
            if ((bytes = receive_message(sock_arg, temp_data)) < 0) {
                printf("Client has disconnected during transfer of big string.");
                break;
            }
            result.append(temp_data);
        }
        return result;
    } catch(const invalid_argument &e)
    {
        cout << "Error while receiving big string" << endl;
    }
    return result;
}

