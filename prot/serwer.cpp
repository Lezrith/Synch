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
    char *data = (char *)calloc(BUFLEN, sizeof(char));
    char *data2 = (char *)calloc(BUFLEN, sizeof(char));
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
    int received;
    char root_path[BUFLEN];
    char file[BUFLEN];
    char temp_name[BUFLEN];
    struct stat helper;
    
    /*remove slash from the end of root directory*/
    for (int i=0; i<BUFLEN; i++)
        if(argv[1][i] == 0){
            if(root_path[i-1] == '/')
                root_path[i-1] = 0;
            break;
        } else
            root_path[i] = argv[1][i];
    printf("root_path=%s\n",root_path);
        
    while(1) {
        int nClientSocket = accept(sock_num, (struct sockaddr*)&stClientAddr, &nTmp);
        if(nClientSocket < 0){
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
            if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0) break;
            printf("received=%d msg=%s\n",received,data);
            
            if(!strcmp(data,"DIRCREATE")){
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0) break;
                printf("received=%d dirname=%s\n",received,data);
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) < 0)
                    mkdir(file, 0644);
                else
                    printf("Directory already exists!\n");
            } 
            
            else if(!strcmp(data,"FILCREATE")){
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0) break;
                printf("received=%d filname=%s\n",received,data);
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) < 0)
                    open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                else
                    printf("File already exists!\n");
            } 
            
            else if(!strcmp(data,"FILDELETE")){
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0) break;
                printf("received=%d filname=%s\n",received,data);
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) == 0)
                    unlink(file);
                else
                    printf("File doesn't exist!\n");
            } 
            
            else if(!strcmp(data,"DIRDELETE")){
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0) break;
                printf("received=%d dirname=%s\n",received,data);
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                if (stat(file, &helper) == 0)
                    rmdir(file);
                else
                    printf("Directory doesn't exist!\n");
            }
            
            else if(!strcmp(data,"BIGDELETE")){
                /*for now server just deletes whole directory,
                when it will be serving many clients, server will have to solve conflicts
                inside this directory */
                /*calling system function isn't fixed, because in the end it won't be done this way*/
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0) break;
                printf("received=%d dirname=%s\n",received,data);
                snprintf(file,BUFLEN,"rm -r \"%s/%s\"", root_path, data);
                system(file);
            } 
            
            else if(!strcmp(data,"MOVED")){
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0) break;
                printf("received=%d moved_from=%s\n",received,data);
                if((received = recv(nClientSocket, data2, BUFLEN, 0)) == 0) break;
                printf("received=%d moved_to=%s\n",received,data2);
                char file_2[BUFLEN];
                snprintf(file, BUFLEN, "%s/%s", root_path, data);
                snprintf(file_2, BUFLEN, "%s/%s", root_path, data2);
                if((stat(file, &helper) == -1) || (stat(file_2, &helper) == 0))
                    printf("Source file doesn't exist or destination file exists!");
                else
                    rename(file, file_2);
            } 
            
            else if(!strcmp(data,"FILMODIFY") || !strcmp(data,"BIGDIR")){
                bool bigdir_arrived = false;
                char bigdir_path[BUFLEN];
                
                if(!strcmp(data,"BIGDIR")) { 
                    /*receive what is the real name of this big dir*/
                    bigdir_arrived = true;
                    if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0){
                        printf("The client has disconnected.\n");
                        break;
                    }
                    printf("received=%d bigdir_path=%s\n",received,data);
                    strncpy(bigdir_path, data, BUFLEN);
                }
            
                /*open the file which is modified*/
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0){
                    printf("The client has disconnected.\n");
                    break;
                }
                printf("received=%d name=%s\n",received,data);
                char file_path[BUFLEN];
                if(bigdir_arrived)
                    snprintf(file_path,BUFLEN, "%s/%s/%s", root_path, bigdir_path, data);
                else
                    snprintf(file_path,BUFLEN, "%s/%s", root_path,data);
                snprintf(temp_name,BUFLEN, "%s",data);
                int fd=open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd<0) perror("open");
                
                /*receive how many chunks are there going to be sent*/
                if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0){
                    printf("The client has disconnected.\nData in file %s has been lost.\n", file_path);
                    close(fd);
                    break;
                }
                printf("received=%d chunks=%s\n",received,data);
                int chunks = atoi(data);
                
                int chunk_size;
                for(int i=1; i<=chunks; i++){
                
                    /*receive chunk size*/
                    if((received = recv(nClientSocket, data, BUFLEN, 0)) == 0){
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
            }
        }
        close(nClientSocket);
    }
    close(sock_num);
    free(data);
    free(data2);
}
