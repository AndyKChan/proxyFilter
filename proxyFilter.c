#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

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
	int MAX_SIZE = 100;

    int main(int argc, char* argv[]){
        
        short portNum;
        int socket_desc, ssocket;
        struct sockaddr_in serverSocket;
        struct sockaddr_in clientSocket;
        FILE *file;
   
    if(argc < 2){
        puts("Input port number");
        exit(1);
    }    

    // Make socket 
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc == -1){
        puts("Socket failed to be created");
        exit(1);
    }
    
    // Handle internet addresses
    serverSocket.sin_family = AF_INET;
    serverSocket.sin_addr.s_addr = INADDR_ANY;
    serverSocket.sin_port = htons(atoi(argv[1]));

    // Initialize blacklist
    
    char *blackList[MAX_SIZE];
    char BLBufferSize[MAX_SIZE]; 
    int BLsize = 0;
    
    char *fileName = argv[2];
    file = fopen(fileName,"r");
    if (!file) {
        puts("No file detected");      
    }

    int counter = 0; 
    while(fgets(BLBufferSize, MAX_SIZE, file) != NULL){
        strtok(BLBufferSize, "\n");
        blackList[counter] = (char*) malloc(MAX_SIZE);
        strcpy(blackList[counter], BLBufferSize);
        BLBufferSize[strlen(BLBufferSize)-1]='\0';
        counter++;
	}
    BLsize = counter; 
    fclose(file);
    
    //Associate server with local address
    if(bind(socket_desc, (struct sockaddr *)&serverSocket, sizeof(serverSocket)) < 0){
        puts("Failed to bind server socket");
        exit(1);
    }
    puts("Bind success");
       
    listen(socket_desc, 40);
       
    //Accept
    puts("Connecting...");
    int c = sizeof(struct sockaddr_in);
       
    while((ssocket = accept(socket_desc, (struct sockaddr *) &clientSocket,(socklen_t*)&c))){

       	int flag = 0; 
      	char buf[4096];
       	char protocol[512];
        int defaultPort = 80; 
        char ports[16]; 
        char remoteURL [512]; 
        char remoteURLPath [512];
        char hostSuffix [512];
        struct sockaddr_in hostSocket;
        int remoteSocket;  
        char reqType[512];    
        char url[4096];
        char parseURL[4096]; 
 
        char* msg = "Proxy Connected. \n";
        write(ssocket,msg,strlen(msg));
       
        recv(ssocket, buf, 4096, flag);
        sscanf(buf, "%s %s %s %s", reqType, url, protocol, hostSuffix);
 
		remoteSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        hostSocket.sin_port = htons(defaultPort);
        hostSocket.sin_addr.s_addr = INADDR_ANY;
        hostSocket.sin_family = AF_INET;
        
        strcpy(parseURL, url);
        printf("%s\n", parseURL);

        char *PathOfInput = &(parseURL[0]);
        char *firstPartOfUrl; 
        char *hostPartOfFile;
        int i; 

        /*
        check for http:// appended to the url or not
        */
        if(strstr(PathOfInput, "http://") != NULL)
        {
          PathOfInput = &(parseURL[6]);
          i += 6;   
        }

        firstPartOfUrl = strtok(PathOfInput, "/");

        if(strstr(firstPartOfUrl, ":") != NULL)
        {
          firstPartOfUrl = strtok(firstPartOfUrl, ":");
          sprintf(remoteURL, "%s", firstPartOfUrl);
          firstPartOfUrl = strtok(NULL, ":");
          sprintf(ports, "%s", firstPartOfUrl);
          defaultPort = atoi(ports);
    
      } else {
             sprintf(remoteURL, "%s", firstPartOfUrl);
      }

       
          PathOfInput = &(url[strlen(remoteURL) + i]);
          sprintf(remoteURLPath, "%s", PathOfInput);
            
        if(strcmp(remoteURLPath, "") == 0)
            {
              sprintf(remoteURLPath, "/");
            }
		 
   int count;
   while(count < BLsize){
   	blackList[count][strlen(blackList[count])-1]='\0';
   			if(strstr(blackList[count]) != NULL){
            	sprintf(BLBufferSize,"403 List check \n%s", blackList[count]);
              	send(ssocket, BLBufferSize, strlen(BLBufferSize), 0);
                exit(1); 
            }
        count++; 
   }

        if(remoteSocket < 0){
          perror("Remote socket failed");  
        }

        if(connect(remoteSocket, (struct sockaddr*)&hostSocket, sizeof(struct sockaddr)) < 0)
        {
            perror("Remote server failed to connect");
            close(remoteSocket); 
            return 0; 
        }

        sprintf(buf,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", remoteURLPath, remoteURL);

        int req = send(remoteSocket, buf, strlen(buf), 0);

        if(req < 0)
        {
            perror("failed to write to remote socket");
        }

}
       
if(ssocket < 0){
  perror("accept failed");
  exit(1);
}

exit(1);
}

void *connection_handler(void* socket_desc){
    int ssocket2 = *(int*) socket_desc;
    char *msg;
    
    msg = "connection handler initiated\n";
    write(ssocket2, msg, strlen(msg));
    
    free(socket_desc);
    return 0;
}