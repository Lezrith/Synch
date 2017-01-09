#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string.h>
#define BUF_SIZE 2048
#define QUEUE_SIZE 1
#define PATH_LEN 2048
#define COMMAND_LEN 2048
using namespace std;

const char *BIGDIR_SPECIAL_NAME = "special_name.tar";
int main(int argc, char **argv) {
    if(argc<4) {
        printf("Usage: %s PATH IP PORT\n",argv[0]);
        exit(EXIT_FAILURE);
    }
	char connect_info[BUF_SIZE];
    char *data = (char *)calloc(BUF_SIZE, sizeof(char));
    char *data2 = (char *)calloc(BUF_SIZE, sizeof(char));
    struct sockaddr_in soc_add, stClientAddr;
	soc_add.sin_family = AF_INET;
	soc_add.sin_addr.s_addr = inet_addr(argv[2]);
	soc_add.sin_port = htons(atoi(argv[3]));
	int sock_num;
	if((sock_num = socket(AF_INET, SOCK_STREAM,0)) < 0)
		perror("socket");
    
    if(bind(sock_num, (struct sockaddr*) &soc_add, sizeof(soc_add)) < 0)
        perror("bind");
    if(listen(sock_num, QUEUE_SIZE) < 0)
        perror("listen");
    socklen_t nTmp = sizeof(struct sockaddr);
    int received;
    char root_path[PATH_LEN];
    char command[COMMAND_LEN];
    char temp_name[PATH_LEN];
    
    /*remove slash from the end of root directory*/
    for (int i=0; i<PATH_LEN; i++)
        if(argv[1][i] == 0){
            if(root_path[i-1] == '/')
                root_path[i-1] = 0;
        } else
            root_path[i] = argv[1][i];
    printf("root_path=%s\n",root_path);
        
    while(1) {
        int nClientSocket = accept(sock_num, (struct sockaddr*)&stClientAddr, &nTmp);
        if(nClientSocket < 0)
            perror("accept");
        
        int n;
        n = sprintf(connect_info, "Connection from %s, port: %d\n", 
            inet_ntoa((struct in_addr)stClientAddr.sin_addr), stClientAddr.sin_port);
        write(1, connect_info, n);
        bool exit_outer_loop = false;
        while(!exit_outer_loop){
            printf("===============================\n");
            if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0) break;
            printf("received=%d msg=%s\n",received,data);
            
            if(!strcmp(data,"DIRCREATE")){
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0) break;
                printf("received=%d dirname=%s\n",received,data);
                snprintf(command,COMMAND_LEN,"mkdir \"%s/%s\"", root_path, data);
                system(command);
            } 
            
            else if(!strcmp(data,"FILCREATE")){
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0) break;
                printf("received=%d filname=%s\n",received,data);
                snprintf(command,COMMAND_LEN,"touch \"%s/%s\"", root_path, data);
                system(command);
            } 
            
            else if(!strcmp(data,"FILDELETE")){
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0) break;
                printf("received=%d filname=%s\n",received,data);
                snprintf(command,COMMAND_LEN,"rm \"%s/%s\"", root_path, data);
                system(command);
            } 
            
            else if(!strcmp(data,"DIRDELETE")){
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0) break;
                printf("received=%d dirname=%s\n",received,data);
                snprintf(command,COMMAND_LEN,"rmdir \"%s/%s\"", root_path, data);
                system(command);
            }
            
            else if(!strcmp(data,"BIGDELETE")){
                /*for now server just deletes whole directory,
                when it will be serving many clients, server will have to solve conflicts
                inside this directory */
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0) break;
                printf("received=%d dirname=%s\n",received,data);
                snprintf(command,COMMAND_LEN,"rm -r \"%s/%s\"", root_path, data);
                system(command);
            } 
            
            else if(!strcmp(data,"MOVED")){
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0) break;
                printf("received=%d moved_from=%s\n",received,data);
                if((received = recv(nClientSocket, data2, BUF_SIZE, 0)) == 0) break;
                printf("received=%d moved_to=%s\n",received,data2);
                snprintf(command,COMMAND_LEN,"mv \"%s/%s\" \"%s/%s\"",
                    root_path, data, root_path, data2);
                system(command);
            } 
            
            else if(!strcmp(data,"FILMODIFY") || !strcmp(data,"BIGDIR")){
                bool bigdir_arrived = false;
                char bigdir_path[PATH_LEN];
                
                if(!strcmp(data,"BIGDIR")) { 
                    /*receive what is the real name of this big dir*/
                    bigdir_arrived = true;
                    if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0){
                        printf("The client has disconnected.\n");
                        break;
                    }
                    printf("received=%d bigdir_path=%s\n",received,data);
                    strncpy(bigdir_path, data, PATH_LEN);
                }
            
                /*open the file which is modified*/
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0){
                    printf("The client has disconnected.\n");
                    break;
                }
                printf("received=%d name=%s\n",received,data);
                char file_path[PATH_LEN];
                if(bigdir_arrived)
                    snprintf(file_path,PATH_LEN,"%s/%s/%s",root_path,bigdir_path,data);
                else
                    snprintf(file_path,PATH_LEN,"%s/%s",root_path,data);
                snprintf(temp_name,PATH_LEN,"%s",data);
                int fd=open(file_path,O_WRONLY|O_CREAT|O_TRUNC, 0644);
                if(fd<0) perror("open");
                
                /*receive how many chunks are there going to be sent*/
                if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0){
                    printf("The client has disconnected.\nData in file %s has been lost.\n", file_path);
                    close(fd);
                    break;
                }
                printf("received=%d chunks=%s\n",received,data);
                int chunks = atoi(data);
                
                int chunk_size;
                for(int i=1; i<=chunks; i++){
                
                    /*receive chunk size*/
                    if((received = recv(nClientSocket, data, BUF_SIZE, 0)) == 0){
                        printf("The client has disconnected during transfer.\n Data in file %s is likely to be corrupted.\n", file_path);
                        exit_outer_loop = true;
                        break;
                    }
                    chunk_size = atoi(data);
                    printf("received=%d chunk_size=%d\n",received,chunk_size);
                    
                    /*if there is data going to be sent, receive it and write to file
                     there may be no data if the file is created only*/
                    if(chunk_size > 0) {
                        if((received = recv(nClientSocket, data, chunk_size, 0)) == 0){
                            printf("The client has disconnected during transfer.\n Data in file %s is likely to be corrupted.\n", file_path);
                            exit_outer_loop = true;
                            break;
                        }
                        printf("received=%d chunk_number=%d\n",received,i);
                        write(fd,data,chunk_size);
                    }
                }
                close(fd);
                
                /*extract the tar into real directory*/
                if(bigdir_arrived){ 
                    printf("file_path=%s\n",file_path);
                    int last_slash = 0;
                    char dest_path[PATH_LEN];
                    int i;
                    for(i=0; i<PATH_LEN; i++){
                        if(file_path[i] == '/')
                            last_slash = i;
                        if(file_path[i] == 0)
                            break;
                    }
                    for(i=0; i<last_slash; i++)
                        dest_path[i] = file_path[i];
                    dest_path[i] = 0;
                    printf("dest_path = %s\n",dest_path);
                        snprintf(command, COMMAND_LEN, "cd \"%s\" && tar -xf \"%s\"",
                            dest_path, temp_name);
                    printf("untar command = %s\n", command);
                    system(command);
                }
            }
        }
        close(nClientSocket);
    }
    close(sock_num);
    free(data);
    free(data2);
}
