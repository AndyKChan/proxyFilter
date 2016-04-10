#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "proxyFilter.h"

// list of black-listed websites
char *url_blacklist[100];
int url_blacklist_len = 0;
int url_index = 0;
FILE *filterFile;
char buf[100];


int main(int argc, char **argv)
{
    pid_t chpid;
    struct sockaddr_in addr_in, cli_addr, serv_addr;
    struct hostent *hostent;
    int sockfd, newsockfd;
    int clilen = sizeof(cli_addr);
    struct stat st = {0};
		
    if(argc != 3)
    {
        printf("Using:\n\t%s <port> <filter-list>\n", argv[0]);
        return -1;
    }

    printf("HTTP Proxy Server now listening on port %i ... \n", atoi(argv[1]));
    
    //Add filter list to the url_blacklist array.
    char *fileName = argv[2];
    filterFile =fopen(fileName,"r");
    if (!filterFile) {
        printf("Failed to load the filter list, Proxy terminates \n");
        return 0;
    }
    
    while (fgets(buf,100, filterFile)!=NULL){
        strtok(buf, "\n");
        url_blacklist[url_index] = (char*) malloc(100);
        strcpy(url_blacklist[url_index], buf);
        url_index += 1;
    }
   
    url_blacklist_len = url_index;
    fclose(filterFile);
    // Done for adding filter list

    // checking if the cache directory exists
    if (stat("./cache/", &st) == -1) {
        mkdir("./cache/", 0700);
    }

    bzero((char*)&serv_addr, sizeof(serv_addr));
    bzero((char*)&cli_addr, sizeof(cli_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // creating the listening socket for our proxy server
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0)
    {
        perror("failed to initialize socket");
    }

    // binding our socket to the given port
    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("failed to bind socket");
    }

    // start listening
    listen(sockfd, 50);

accepting:
    newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

    if((chpid = fork()) == 0)
    {
        struct sockaddr_in host_addr;
        int i, n;           // loop indices
        int rsockfd;        // remote socket file descriptor
        int cfd;            // cache-file file descriptor

        int port = 80;      // default http port - can be overridden
        char type[256];     // type of request - e.g. GET/POST/HEAD
        char url[4096];     // url in request - e.g. facebook.com
        char proto[256];    // protocol ver in request - e.g. HTTP/1.1

        char datetime[256]; // the date-time when we last cached a url

        // break-down of a given url by parse_url function
        char url_host[256], url_path[256];

        char url_encoded[4096]; // encoded url, used for cahce filenames
        char filepath[256];     // used for cache file paths

        char *dateptr;      // used to find the date-time in http response
        char buffer[4096];  // buffer used for send/receive
        int response_code;  // http response code - e.g. 200, 304, 301

        bzero((char*)buffer, 4096);

        // recieving the http request
        recv(newsockfd, buffer, 4096, 0);

        // we only care about the first line in request
        sscanf(buffer, "%s %s %s", type, url, proto);

        // adjusting the url -- some cleanup!
        if(url[0] == '/')
        {
            strcpy(buffer, &url[1]);
            strcpy(url, buffer);
        }
        
        // Only GET requests are accepted
        if((strncmp(type , "GET", 3) != 0) || ((strncmp(proto, "HTTP/1.1", 8) != 0)))
        {
            // invalid request -- send the following line back to browser
            sprintf(buffer,"405 : BAD REQUEST\nONLY GET REQUESTS ARE ALLOWED");
            send(newsockfd, buffer, strlen(buffer), 0);
            goto end;
        }

        // Break down the url to know the host and path
        parse_url(url, url_host, &port, url_path);
        // encoding the url for later use
        url_encode(url, url_encoded);

        // BLACK LIST CHECK
        for(i = 0; i < url_blacklist_len; i++)
        {
            //printf("url_blacklist[%i]: %s \n", i, url_blacklist[i]);
            // if url contains the black-listed word
            if(NULL != strstr(url, url_blacklist[i]))
            {
                
                sprintf(buffer,"403 : BAD REQUEST\nURL FOUND IN BLACKLIST\n%s", url_blacklist[i]);
                send(newsockfd, buffer, strlen(buffer), 0);
                goto end;
            }
        }
        // Find the ip for the host
        if((hostent = gethostbyname(url_host)) == NULL)
        {
            fprintf(stderr, "failed to resolve %s: %s\n", url_host, strerror(errno));
            goto end;
        }

        bzero((char*)&host_addr, sizeof(host_addr));
        host_addr.sin_port = htons(port);
        host_addr.sin_family = AF_INET;
        bcopy((char*)hostent->h_addr, (char*)&host_addr.sin_addr.s_addr, hostent->h_length);

        // create a socket to connect to the remote host
        rsockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if(rsockfd < 0)
        {
            perror("failed to create remote socket");
            goto end;
        }

        // try connecting to the remote host
        if(connect(rsockfd, (struct sockaddr*)&host_addr, sizeof(struct sockaddr)) < 0)
        {
            perror("failed to connect to remote server");
            goto end;
        }

        // CACHING CHECK
        sprintf(filepath, "./cache/%s", url_encoded);
        if (0 != access(filepath, 0)) {
            // Don't have any file by this name, request from the remote host
            sprintf(buffer,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", url_path, url_host);
            goto request;
        }

        // The request has been cached previously
        // open the file
        sprintf(filepath, "./cache/%s", url_encoded);
        cfd = open (filepath, O_RDWR);
        bzero((char*)buffer, 4096);
        read(cfd, buffer, 4096);
        close(cfd);

        // Find the date it was cached
        dateptr = strstr(buffer, "Date:");
        if(NULL != dateptr)
        {
            // response has a Date field
            bzero((char*)datetime, 256);
            strncpy(datetime, &dateptr[6], 29);

            // send CONDITIONAL GET
            // If-Modified-Since the date that we cached it
            sprintf(buffer,"GET %s HTTP/1.1\r\nHost: %s\r\nIf-Modified-Since: %s\r\nConnection: close\r\n\r\n", url_path, url_host, datetime);

        } else {
            sprintf(buffer,"GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", url_path, url_host);
        }

request:
        // send the request to remote host
        n = send(rsockfd, buffer, strlen(buffer), 0);

        if(n < 0)
        {
            perror("failed to write to remote socket");
            goto end;
        }

do_cache:
        // Cache the requested website for later uses
        cfd = -1;

        do
        {
            bzero((char*)buffer, 4096);

            // recieve from remote host
            n = recv(rsockfd, buffer, 4096, 0);
            // if we have read anything - otherwise END-OF-FILE
            if(n > 0)
            {
                if(cfd == -1)
                {
                    float ver;
                    sscanf(buffer, "HTTP/%f %d", &ver, &response_code);

                    // if it is not 304 -- anything other than sub-CASE32
                    if(response_code != 304)
                    {
                        // create the cache-file to save the content
                        sprintf(filepath, "./cache/%s", url_encoded);
                        if((cfd = open(filepath, O_RDWR|O_TRUNC|O_CREAT, S_IRWXU)) < 0)
                        {
                            perror("failed to create cache file");
                            goto end;
                        }
                    } else {
                        // send the response to the browser from local cache
                        goto from_cache;
                    }
                }

                // write to file
                write(cfd, buffer, n);
            }
        } while(n > 0);
        close(cfd);

from_cache:

        // Cache found, read from cache file
        sprintf(filepath, "./cache/%s", url_encoded);
        if((cfd = open (filepath, O_RDONLY)) < 0)
        {
            perror("failed to open cache file");
            goto end;
        }
        do
        {
            bzero((char*)buffer, 4096);
            n = read(cfd, buffer, 4096);
            if(n > 0)
            {
                // send it to the browser
                send(newsockfd, buffer, n, 0);
            }
        } while(n > 0);
        close(cfd);

end:
        // closing sockets!
        close(rsockfd);
        close(newsockfd);
        close(sockfd);
        return 0;
    } else {
        // closing socket
        close(newsockfd);
        // loop...
        goto accepting;
    }

    close(sockfd);
    return 0;
}