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

#define BACKLOG 10 
void *connection_handler(void*);

//gets socket address, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}




int main(int argc, char* argv[]){
    int sockfd, newSocket, status;
    int MAX_SIZE = 100;
    short portNum;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    FILE *file;
    int flag = 1;
    struct addrinfo hints, *servinfo, *p, *res;
    
    char s[INET6_ADDRSTRLEN];
    
    //check # args
    if(argc < 2){
        puts("Arguments should be more than 2");
        exit(1);
    }   

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Forces IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv;
    //NULL for nodename b/c bind fills it in later
    if((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0){
    	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    	exit(1);
    }

	//loop through all results and connect to the first we can
   for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
        perror("socket");
        continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        close(sockfd);
        perror("bind");
        continue;
    }

    break; 
	}

freeaddrinfo(servinfo); // all done with this structure

// looped off the end of the list with no connection
if (p == NULL) {
    fprintf(stderr, "failed to connect\n");
    exit(2);
}

  if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }


   
   puts("Server: Connecting...");
    

    // Initialize blacklist
    
    char *blackList[MAX_SIZE];
    char BLBufferSize[MAX_SIZE]; 
    int BLsize = 0;
    
    char *fileName = argv[2];
    file = fopen(fileName,"r");
    if (!file) {
        puts("No file detected");      
    }

    int counter; 
    while(fgets(BLBufferSize, MAX_SIZE, file) != NULL){
        strtok(BLBufferSize, "\n");
        blackList[counter] = (char*) malloc(MAX_SIZE);
        strcpy(blackList[counter], BLBufferSize);
        BLBufferSize[strlen(BLBufferSize)-1]='\0';
        counter++;
  }
    BLsize = counter; 
    fclose(file);
    
    while(1){
    	int sin_size = sizeof their_addr;
    	newSocket = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    	if(newSocket == -1) {
    		perror("accept");
    		continue;
    	}
    	inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("Server: got connection from %s\n", s);

        char* msg = "Proxy Connected. \n";
        write(newSocket,msg,strlen(msg));

   		char buf[4096];
   		char requestType[256];    
		char url[4096], urlHost[256], urlPath[256];
		char protocol[256];
		char hostSuffix [256];

        int flag = 0; 
        int port = 80; 

        
        struct sockaddr_in hostSocket; 
        int remoteSocket;  
        char parseURL[4096], hostName[1024],statedPort[16]; //to change
        char* URLfrag = NULL; //to change

		char ports[16]; 
        char remoteURL [256]; 
        char remoteURLPath [256];
        //get request
        recv(newSocket, buf, 4096, 0);
		sscanf(buf, "%s %s %s %s", requestType, url, protocol, hostSuffix);
		 if(url[0] == '/')
        {
            strcpy(buf, &url[1]);
            strcpy(url, buf);
        }
        
        if((strncmp(requestType, "GET", 3) != 0)){
        	sprintf(buf, "405: GET REQUEST ONLY");
        	send(newSocket, buf, strlen(buf) , 0);
        	exit(1);
        }

        strcpy(parseURL, url); 	
        char *inputPath = &(parseURL[0]);
 		
        int inc; 
        //find http:// section in inputPath and increment
        if(strstr(inputPath, "http://") != NULL){
          inputPath = &(parseURL[6]);
          inc += 6;   
        }
        //get hostName
        URLfrag = strtok(inputPath, "/");
        sprintf(hostName, "%s", URLfrag);

        //check if port stated
        if(strstr(hostName, ":") != NULL){
        	URLfrag = strtok(hostName, ":");
        	sprintf(urlHost, "%s", URLfrag);
			URLfrag = strtok(NULL, ":");
			sprintf(statedPort, "%s", URLfrag);
			port = atoi(statedPort);
		} else {
		sprintf(urlHost, "%s", hostName);
		}

		// inputPath = &(url[strlen(hostName) + inc]);
		// sprintf(urlPath, "%s", inputPath);
		// if(strcmp(urlPath, "") == 0)
		// {
		// 	sprintf(urlPath, "/");
		// }
	
		//blacklist portion
   int count = 0;
   while(count < BLsize){
    blackList[count][strlen(blackList[count])-1]='\0';
        if(strstr(blackList[count], url) != NULL){
              sprintf(BLBufferSize,"403 List check \n%s", blackList[count]);
                send(newSocket, BLBufferSize, strlen(BLBufferSize), 0);
                exit(1); 
            }
        count++; 
   }

        if(remoteSocket < 0){
          perror("Remote socket failed");  
        }

        // if(connect(remoteSocket, get_in_addr(hostSocket), sizeof(struct sockaddr)) < 0)
        // {
        //     perror("Remote server failed to connect");
        //     close(remoteSocket); 
        //     return 0; 
        // }

        sprintf(buf,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", remoteURLPath, remoteURL);

        int req = send(remoteSocket, buf, strlen(buf), 0);

        if(req < 0)
        {
            perror("failed to write to remote socket");
        }

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(newSocket, "Hello, world!", 13, 0) == -1)
                perror("send");
            close(newSocket);
            exit(0);
        }
        close(newSocket);
	}
	return 0;
}
       
