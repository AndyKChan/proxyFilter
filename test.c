#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

void *connection_handler(void*);

    int main(int argc, char* argv[]){
        
        short port;
        int socket_desc, new_socket;
        struct sockaddr_in server;
        struct sockaddr_in client;
        FILE *file;


        
        if(argc < 2){
            puts("enter a port number");
            exit(1);
        }
    

        // create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc == -1){
        printf("Could not create socket");
        exit(1);
    }
    
        // Make sockaddr to bind the socket to
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(atoi(argv[1]));


    char *blacklist[100];
    int blacklist_size = 0;
    char blacklistBuffer[100]; 

    char *fileInput = argv[2];
    file =fopen(fileInput,"r");
    if (!file) {
        printf("No file detected \n");      
    }

    int j; 
    while (fgets(blacklistBuffer,100, file) != NULL){
        strtok(blacklistBuffer, "\n");
        blacklist[j] = (char*) malloc(100);
        strcpy(blacklist[j], blacklistBuffer);
     
        blacklistBuffer[strlen(blacklistBuffer)-1]='\0';
        j++;

}
    blacklist_size = j; 
    printf("ddddd %d\n", blacklist_size);
    fclose(file);
    
       
       // bind
       if(bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0){
           puts("bind failed");
           exit(1);
       }
       puts("bind done");
       
       //LISTEN
       listen(socket_desc, 3);
       
       
       //Accept
       puts("waiting for connection");
       int c = sizeof(struct sockaddr_in);
       
       while((new_socket = accept(socket_desc, (struct sockaddr *) &client,(socklen_t*)&c))){

       int flags = 0; 
       char buf[4096];

       struct sockaddr_in host_sock;
       int remote_sock;  
       char reqType[256];    
       char url[4096];
       char parse_url[4096]; //for parsing
     
    
       char protocol[256];
       int req; 
       int port = 80; 
       char portStore[16]; 
       char remote_url [256]; 
       char remote_url_path [256];
       char hostSuffix [256];
       char actualHost [4096];   
       puts("connection accepted");

       //RESPONSE
       char* message = "Hello! You have connected to the proxy \n";
       write(new_socket,message,strlen(message));
       
       recv(new_socket, buf, 4096, flags);
       sscanf(buf, "%s %s %s %s %s", reqType, url, protocol, hostSuffix, actualHost);

printf("d %s\n", actualHost);

 
        host_sock.sin_port = htons(port);
        host_sock.sin_family = AF_INET;
        host_sock.sin_addr.s_addr = INADDR_ANY;

    
        remote_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        strcpy(parse_url, url);
        printf("%s\n", parse_url);


        char *input_path = &(parse_url[0]);
        char *firstPartOfUrl; 
        char *hostPartOfFile;
        int i; 

        /*
        check for http:// appended to the url or not
        */
        if(strstr(input_path, "http://") != NULL)
        {
          input_path = &(parse_url[6]);
          i += 6;   
        }

        firstPartOfUrl = strtok(input_path, "/");

        if(strstr(firstPartOfUrl, ":") != NULL)
        {
          firstPartOfUrl = strtok(firstPartOfUrl, ":");
          sprintf(remote_url, "%s", firstPartOfUrl);
          firstPartOfUrl = strtok(NULL, ":");
          sprintf(portStore, "%s", firstPartOfUrl);
          port = atoi(portStore);
    
      } else {
             sprintf(remote_url, "%s", firstPartOfUrl);
      }

       
          input_path = &(url[strlen(remote_url) + i]);
          sprintf(remote_url_path, "%s", input_path);
            
        if(strcmp(remote_url_path, "") == 0)
            {
              sprintf(remote_url_path, "/");
            }




  int z; 
 
/*
Check if the requested host contains any blacklisted strings
Params: @actualHost = requested hostname
        @blacklist[] = array of blacklisted strings to be searched
*/

   while(z < blacklist_size){
   blacklist[z][strlen(blacklist[z])-1]='\0';
   if(strstr(actualHost, blacklist[z]) != NULL){
            sprintf(blacklistBuffer,"403 List check \n%s", blacklist[z]);
              send(new_socket, blacklistBuffer, strlen(blacklistBuffer), 0);
                return 0; 
            }
        z++; 
        }


        if(remote_sock < 0){
          perror("socket failure");  
        }

   
        if(connect(remote_sock, (struct sockaddr*)&host_sock, sizeof(struct sockaddr)) < 0)
        {
            perror("remote server connection failure");
            close(remote_sock); 
            return 0; 
         
        }

      
        sprintf(buf,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", remote_url_path, remote_url);
        req = send(remote_sock, buf, strlen(buf), 0);

        if(req < 0)
        {
            perror("failed to write to remote socket");
        }

       }
       
       if(new_socket < 0){
           perror("accept failed");
           exit(1);
       }

    return 0;
}

void *connection_handler(void* socket_desc){
    int sock = *(int*) socket_desc;
    char *message;
    
    message = "connection handler initiated\n";
    write(sock, message, strlen(message));
    
    free(socket_desc);
    return 0;
}