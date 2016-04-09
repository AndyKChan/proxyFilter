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

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
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
    struct sigaction sa;
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

     sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
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

    

        int flag = 0; 
        char buf[4096];
        char protocol[256];
        int defaultPort = 80; 
        char ports[16]; 
        char remoteURL [256]; 
        char remoteURLPath [256];
        char hostSuffix [256];
        struct sockaddr_in hostSocket; 
        int remoteSocket;  
        char reqType[256];    
        char url[4096];
        char parseURL[4096]; 
 
        char* msg = "Proxy Connected. \n";
        write(newSocket,msg,strlen(msg));
       
        recv(newSocket, buf, 4096, flag);
        sscanf(buf, "%s %s %s %s", reqType, url, protocol, hostSuffix);
		strcpy(parseURL, url);
        printf("%s\n", parseURL);
 
 		remoteSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        hostSocket.sin_port = htons(defaultPort);
        hostSocket.sin_addr.s_addr = INADDR_ANY;
        hostSocket.sin_family = AF_INET;
     

        char *PathOfInput = &(parseURL[0]);
        char *hostName; 
        int i; 
        //find http:// section in pathofinput and increment
        if(strstr(PathOfInput, "http://") != NULL)
        {
          PathOfInput = &(parseURL[6]);
          i += 6;   
        }

        hostName = strtok(PathOfInput, "/");

        printf("Host name: %s\n" , hostName);

        if(strstr(hostName, ":") != NULL)
        {
          hostName = strtok(hostName, ":");
          
          sprintf(remoteURL, "%s", hostName);
          hostName = strtok(NULL, ":");
          
          sprintf(ports, "%s", hostName);
          defaultPort = atoi(ports);
    
      } else {
             sprintf(remoteURL, "%s", hostName);
      }

       
          PathOfInput = &(url[strlen(remoteURL) + i]);
          sprintf(remoteURLPath, "%s", PathOfInput);
            
        if(strcmp(remoteURLPath, "") == 0)
            {
              sprintf(remoteURLPath, "/");
            }
     
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
       
