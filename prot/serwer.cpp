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
#define BUFLEN 2048
#define QUEUE_SIZE 1
using namespace std;

char *data;
char *data2;

/* in case of error returns negative number, otherwise number of bytes received*/
int receive_message(int sock_arg, char *data_arg){
    int value;
    int *ptr = &value;  
    int received;
    if((received = recv(sock_arg, ptr, sizeof(int), 0)) == 0) return -1;
    printf("received=%d bytes=%d\n", received, *ptr);
    for(int i=0; i<BUFLEN; i++)
        data_arg[i] = 0;
    if((received = recv(sock_arg, data_arg, *ptr, 0)) == 0) return -2;
    if(*ptr < 256)
        printf("received=%d message=%s\n", received, data_arg);
    return *ptr;
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
    
	char connect_info[BUFLEN];
    data = (char *)calloc(BUFLEN, sizeof(char));
    data2 = (char *)calloc(BUFLEN, sizeof(char));
    struct sockaddr_in soc_add, stClientAddr;
	soc_add.sin_family = AF_INET;
	soc_add.sin_addr.s_addr = inet_addr(argv[2]);
	soc_add.sin_port = htons(port_num);
	
	int sock_num;
	if((sock_num = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		exit(EXIT_FAILURE);
    }
    if(bind(sock_num, (struct sockaddr*) &soc_add, sizeof(soc_add)) < 0){
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if(listen(sock_num, QUEUE_SIZE) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }
    socklen_t nTmp = sizeof(struct sockaddr);
    char root_path[BUFLEN];
    char file[BUFLEN];
    char temp_name[BUFLEN];
    char message_type[BUFLEN];
    struct stat helper;
    
    /* remove slash from the end of root directory */
    for (int i=0; i<BUFLEN; i++)
        if(argv[1][i] == 0){
            if(root_path[i-1] == '/')
                root_path[i-1] = 0;
            break;
        } else
            root_path[i] = argv[1][i];
    printf("root_path=%s\n",root_path);
        
    while(1) {
        int client_sock_num = accept(sock_num, (struct sockaddr*)&stClientAddr, &nTmp);
        if(client_sock_num < 0){
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        int n;
        n = sprintf(connect_info, "Connection from %s, port: %d\n", 
            inet_ntoa((struct in_addr)stClientAddr.sin_addr), stClientAddr.sin_port);
        write(1, connect_info, n);
        bool exit_outer_loop = false;
        
        while(!exit_outer_loop){
            printf("===============================\n");
            if(receive_message(client_sock_num, data) < 0)
                break;
            strncpy(message_type, data, BUFLEN);
            if(!strcmp(message_type, "DIRCREATE")){
                if(receive_message(client_sock_num, data) < 0)
                    break;
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) < 0)
                    mkdir(file, 0755);
                else
                    printf("Directory already exists!\n");
            } 
            
            else if(!strcmp(message_type, "FILCREATE")){
                if(receive_message(client_sock_num, data) < 0)
                    break;
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) < 0)
                    open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                else
                    printf("File already exists!\n");
            } 
            
            else if(!strcmp(message_type, "FILDELETE")){
                if(receive_message(client_sock_num, data) < 0)
                    break;
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) == 0)
                    unlink(file);
                else
                    printf("File doesn't exist!\n");
            } 
            
            else if(!strcmp(message_type, "DIRDELETE")){
                if(receive_message(client_sock_num, data) < 0)
                    break;
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) == 0)
                    rmdir(file);
                else
                    printf("Directory doesn't exist!\n");
            }
            
            /* begin of FIXME */
            else if(!strcmp(message_type, "BIGDELETE")){
                /* for now server just deletes whole directory,
                when it will be serving many clients, server will have to solve conflicts
                inside this directory */
                /* calling system function isn't fixed, because in the end
                it won't be done this way */
                if(receive_message(client_sock_num, data) < 0)
                    break;
                snprintf(file,BUFLEN,"rm -r \"%s/%s\"", root_path, data);
                system(file);
            }
            /* end of FIXME */
            
            else if(!strcmp(message_type, "MOVED")){
                if(receive_message(client_sock_num, data) < 0)
                    break;
                if(receive_message(client_sock_num, data2) < 0)
                    break;
                char file_2[BUFLEN];
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                snprintf(file_2, BUFLEN, "%s/%s", root_path, data2);
                if((stat(file, &helper) == -1) || (stat(file_2, &helper) == 0))
                    printf("Source file doesn't exist or destination file exists!");
                else
                    rename(file, file_2);
            } 
            
            else if(!strcmp(message_type, "FILMODIFY") || !strcmp(message_type, "BIGDIR")){
                bool bigdir_arrived = false;
                char bigdir_path[BUFLEN];
                
                /* begin of FIXME */
                if(!strcmp(message_type, "BIGDIR")) { 
                    /* receive what is the real name of this big dir */
                    bigdir_arrived = true;
                    if(receive_message(client_sock_num, data) < 0){
                        printf("The client has disconnected.\n");
                        break;
                    }
                    strncpy(bigdir_path, data, BUFLEN);
                }
                /* end of FIXME */
            
                /* open the file which is modified */
                if(receive_message(client_sock_num, data) < 0){
                    printf("The client has disconnected.\n");
                    break;
                }
                char file_path[BUFLEN];
                if(bigdir_arrived)
                    snprintf(file_path, BUFLEN, "%s/%s/%s", root_path, bigdir_path, data);
                else
                    snprintf(file_path, BUFLEN, "%s/%s", root_path, data);
                snprintf(temp_name,BUFLEN, "%s",data);
                int fd=open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd<0) perror("open");
                
                /* receive how many chunks are there going to be sent */
                if(receive_message(client_sock_num, data) < 0){
                    printf("The client has disconnected.\nData in file %s has been lost.\n", file_path);
                    close(fd);
                    break;
                }
                int chunks = atoi(data);
                printf("chunks = %d\n", chunks);
                int bytes;
                
                for(int i=1; i<=chunks; i++){
                        if((bytes = receive_message(client_sock_num, data)) < 0){
                            printf("The client has disconnected during transfer.\n Data in file %s is likely to be corrupted.\n", file_path);
                            exit_outer_loop = true;
                            break;
                        }
                        printf("chunk_number=%d\n", i);
                        write(fd, data, bytes);
                }
                close(fd);
                
                /* begin of FIXME */
                /* extract the tar into real directory */
                if(bigdir_arrived){ 
                    printf("file_path=%s\n",file_path);
                    int last_slash = 0;
                    char dest_path[BUFLEN];
                    int i;
                    for(i=0; i<BUFLEN; i++){
                        if(file_path[i] == '/')
                            last_slash = i;
                        if(file_path[i] == 0)
                            break;
                    }
                    for(i=0; i<last_slash; i++)
                        dest_path[i] = file_path[i];
                    dest_path[i] = 0;
                    printf("dest_path = %s\n",dest_path);
                        snprintf(file, BUFLEN, "cd \"%s\" && tar -xf \"%s\"",
                            dest_path, temp_name);
                    printf("untar command = %s\n", file);
                    system(file);
                }
                /* end of FIXME */
            }
        }
        close(client_sock_num);
    }
    close(sock_num);
    free(data);
    free(data2);
}
