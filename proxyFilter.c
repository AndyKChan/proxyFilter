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
#include <time.h>

#define BACKLOG 10
#define MAX_SIZE 100

#define CACHE_SIZE 1000 // Maximum entries for the cache

void *connection_handler(void*);
void *get_in_addr(struct sockaddr *sa);
void hash_uri(char* hash, char* input);

char *blackList[MAX_SIZE];
int BLsize = 0;

char** cache;

int main(int argc, char* argv[]){
  int sockfd, status;
  short portNum;
  socklen_t sin_size;
  FILE *file;
  int flag = 1;
  struct addrinfo hints, *servinfo, *p, *res;

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
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) == -1) {
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

  char BLBufferSize[MAX_SIZE];

  char *fileName = argv[2];
  file = fopen(fileName,"r");
  if (!file) {
    puts("No file detected");
    exit(1);
  }

  //find blacklist file size
  int counter = 0;
  while(fgets(BLBufferSize, MAX_SIZE, file) != NULL){
    strtok(BLBufferSize, "\n");
    blackList[counter] = (char*) malloc(MAX_SIZE + 1);
    strcpy(blackList[counter], BLBufferSize);
    BLBufferSize[strlen(BLBufferSize)-1]='\0';
    counter++;
  }
  BLsize = counter;
  fclose(file);

  // initialize cache array
  cache = malloc(CACHE_SIZE * sizeof(char *));

  //Create 4 worker threads
  pthread_t tid[4];
  int err;
  int i = 0;
  while (i < 4) {
 	err = pthread_create(&tid[i], NULL, &connection_handler, (void*) &sockfd);
  	i++;
  }
  pthread_join(tid[0], NULL);
  pthread_join(tid[1], NULL);
  pthread_join(tid[2], NULL);
  pthread_join(tid[3], NULL);

  return 0;
}

void* connection_handler(void* socket_desc) {

  int sockfd = *((int *) socket_desc);

  while (1) {
    struct sockaddr_in hostAddr;
    struct sockaddr_storage their_addr;
    char s[INET6_ADDRSTRLEN];
    int newSocket;
    int req;
    int sin_size = sizeof their_addr;
    newSocket = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if(newSocket == -1) {
      perror("accept");
      continue;
    }
    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("Server: got connection from %s\n", s);

    char buf[4096];
    bzero((char*)buf, 4096);
    char requestType[256];
    char url[4096], urlHost[256], urlPath[256];
    char protocol[256];
    char hostSuffix [256];

    int flags = 0;
    int port = 80;

    struct sockaddr_in hostSocket;
    int remoteSock;
    int cacheDesc;
    char parseURL[1024], hostName[1024],statedPort[16];
    char* URLfrag = NULL;

    char ports[16];
    char remoteURL [256];
    char remoteURLPath [256];

    // GET request
    recv(newSocket, buf, 4096, 0);
    sscanf(buf, "%s %s %s %s", requestType, url, protocol, hostSuffix);
    if(url[0] == '/') {
      strcpy(buf, &url[1]);
      strcpy(url, buf);
    }

    //check if request is GET, otherwise lose connection
    if((strncmp(requestType, "GET", 3) != 0)){
      sprintf(buf, "405: GET REQUEST ONLY");
      send(newSocket, buf, strlen(buf) , 0);

      close(newSocket);
      continue;
    }

    int inc = 1;

    // make new URL to work on
    strcpy(parseURL, url);
    char *inputPath = &(parseURL[0]);

    //check if URL begins with http://, increment if true
    if(NULL != strstr(inputPath, "http://")){
      inputPath = &(parseURL[6]);
      inc += 6;
    }

    URLfrag = strtok(inputPath, "/");
    sprintf(hostName, "%s", URLfrag);

    if(NULL != strstr(hostName, ":")) {
      URLfrag = strtok(hostName, ":");
      sprintf(urlHost, "%s", URLfrag);
      URLfrag = strtok(NULL, ":");
      sprintf(statedPort, "%s", URLfrag);
      port = atoi(statedPort);
    }
    else {
      sprintf(urlHost, "%s", hostName);
    }

    // modify URL path by inc
    inputPath = &(url[strlen(urlHost) + inc]);

    sprintf(urlPath, "%s", inputPath);

    if(strcmp(urlPath, "") == 0) {
      sprintf(urlPath, "/");
    }

    //check if URL contains any black listed strings
    int count;
    for(count = 0; count < BLsize; count++) {
      if(strstr(url, blackList[count]) != NULL){
        sprintf(buf,"403 : URL found in black list, connection closed\n%s", blackList[count]);
        send(newSocket, buf, strlen(buf), 0);
        exit(1);
      }
    }

    char hashed[33];
    int cached = 0;
    hash_uri(hashed ,"abcd");

    // check if inputPath is cached
    for (count = 0; count < CACHE_SIZE; count++) {
      char* cache_entry = cache[count];
      if (cache_entry == NULL) {
        continue;
      }

      if (strcmp(cache_entry, hashed) == 0) {
        // there is a match
        cached = 1;
        break;
      }
    }

    if (!cached) {
      // create remote socket to talk to destination
      remoteSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

      if(remoteSock < 0){
        perror("Remote socket failed");
        exit(0);
      }


      struct hostent *hostPortion;
      // GET IP for host
      if((hostPortion = gethostbyname(urlHost)) == NULL){
        fprintf(stderr, "Cannot find IP %s: %s\n", urlHost, strerror(errno));
        exit(0);
      }

      bzero((char*)&hostAddr, sizeof(hostAddr));
      hostAddr.sin_port = htons(port);
      hostAddr.sin_family = AF_INET;
      bcopy((char*)hostPortion->h_addr, (char*)&hostAddr.sin_addr.s_addr, hostPortion->h_length);

      // connect to remote socket
      if (connect(remoteSock, (struct sockaddr*)&hostAddr, sizeof(struct sockaddr)) < 0){
        perror("Remote server failed to connect");
        exit(0);
      }

      // GET request from remote host
      sprintf(buf,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", urlPath, urlHost);

      // send request to remote host
      req = send(remoteSock, buf, strlen(buf), flags);
      printf("%s", buf);

      if(req < 0) {
        perror("Failed to send req to remote host");
        exit(1);
      }

      // Copying response to cache_file
      char cache_file_path[40];
      strcpy(cache_file_path, "cache/");
      strcat(cache_file_path, hashed);

      FILE* cache_file = fopen(cache_file_path, "w"); // open cache_file to write

      // while there is a request, remote host receives request and sends data to client
      while(req > 0) {
        char* buf = malloc(sizeof(char) * 8193);
        bzero(buf, 8193);

        req = recv(remoteSock, buf, 8192, 0);
        // sends data to client
        send(newSocket, buf, req, 0);

        // also, append to cached file
        fputs(buf, cache_file);

        if (req == 0) break;
      }

      close(remoteSock);
      fclose(cache_file);

      // put the hash into the cache array after we've written to file
      int j;
      int written = 0;
      for (j = 0; j < CACHE_SIZE; j++) {
        if (cache[j] == NULL) { // found an empty slot in the array
          cache[j] = malloc(sizeof(char) * 33);
          strcpy(cache[j], hashed);

          written = 1;
          break;
        }
      }

      if (!written) {
        // oops, ran out of space in array, then purge the first element
        cache[0] = malloc(sizeof(char) * 33);
        strcpy(cache[0], hashed);
      }

    } else {
      // request is cached in a file named hash
      printf("Cache found for request\n");

      // read from the file containing the cached response and relay to client
      char cache_file_path[40];
      strcpy(cache_file_path, "cache/");
      strcat(cache_file_path, hashed);

      FILE* cached_file = fopen(cache_file_path, "r");

      char temp_buffer[8193];
      while (1) {
        bzero(temp_buffer, 8193);
        int bytes_read = fread(temp_buffer, 1, 8192, cached_file);

        printf("%.20s\n", temp_buffer + 8172);
        printf("%d\n", bytes_read);

        if (bytes_read == 0) {
          break;
        }

        // sends data to client
        send(newSocket, temp_buffer, 8192, 0);

        if (bytes_read != 8192)
          break;
      }
    }

    close(newSocket);

  }
}

/* custom URI hashing function, makes a 32 char string */
void hash_uri(char* hash, char* input)
{
  char* characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  strcpy(hash, "feViuKB2q0aGyYdSy5cYxK16kNrQ3NZu");
  char* strptr = input;

  while (*strptr) {
    char* seedptr = hash;
    while (*seedptr) {
      unsigned int sum = (unsigned int) *seedptr + (unsigned int) *strptr;
      sum = sum % 62; // 43 alpha numeric characters
      *seedptr = characters[sum];

      seedptr += 1;
    }
    strptr++;
  }
}

//gets socket address, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
