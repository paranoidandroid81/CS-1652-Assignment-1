#include "minet_socket.h"
#include <stdlib.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <iostream>
#include <string>

#define BUFSIZE 1024

int main(int argc, char * argv[]) { 
    char * server_name = NULL;
    int server_port    = -1;
    char * server_path = NULL;
    char * req         = NULL;
    bool ok            = false;

    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    int path_len = strlen("GET  HTTP/1.0\r\n\r\n") + strlen(server_path);
    req = (char *)calloc(path_len + 1, sizeof(char));
	
    /* initialize */
    if (toupper(*(argv[1])) == 'K') {
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') {
       exit(-1); // not implementing this yet
	     minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    int min_sock = minet_socket(SOCK_STREAM);

    struct hostent *hostentry = gethostbyname(server_name);

    struct sockaddr_in hostsockaddr;
    hostsockaddr.sin_family = AF_INET;
    hostsockaddr.sin_port = htons(server_port);
    memcpy((void *)&(hostsockaddr.sin_addr.s_addr),(void *)hostentry->h_addr, sizeof(hostentry->h_addr));

    /* connect to the server socket */
    if (minet_connect(min_sock, &hostsockaddr) < 0) {
      fprintf(stderr, "Failed at connect");
      exit(-1);
    }

    sprintf(req, "GET %s HTTP/1.0\r\n\r\n", server_path);

    if (minet_write(min_sock, req, path_len) < path_len) {
	  minet_perror("Error: Could not write full HTTP request");
      exit(-1);
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(min_sock, &readfds);
    /* wait till socket can be read. */

    if(minet_select(min_sock + 1, &readfds, NULL, NULL, NULL) < 1) {
      std:: cout << "Got nothing from select!" << std::endl;
	  exit(-1);
    }

    char * buffer = (char*)calloc(BUFSIZE + 1, sizeof(char));

    /* first read loop -- read headers */

    int buff_len = minet_read(min_sock, buffer, BUFSIZE);
    if (buff_len < 1) {
        std::cout << "Could not read http header!" << std::endl;
		exit(-1);
    }
    
    std::string response(buffer);

    std::string header = response.substr(9);

    ok = (header.substr(0, 3).compare("200") == 0);
    // Normal reply has return code 200

    /* print first part of response: header, error code, etc. */

    //Read from end of header, skipping \r\n\r\n
	
	FD_ZERO(&readfds);
    FD_SET(min_sock, &readfds);
	memset(buffer, '\0', sizeof(char) * 1025);
	
	/* second read loop -- print out the rest of the response: real web content */
    while(minet_select(min_sock + 1, &readfds, NULL, NULL, NULL) > 0){
		
		if (!FD_ISSET(min_sock, &readfds)){
			std::cout << "There was an error reading from the server socket!" << std::endl;
			exit(-1);
		}
		
		buff_len = minet_read(min_sock, buffer, BUFSIZE);
		if (buff_len < 1) break;
		
		std::cout << buffer << std::endl;
		FD_ZERO(&readfds);
		FD_SET(min_sock, &readfds);
		memset(buffer, '\0', sizeof(char) * 1025);
	}
    
	ok = (minet_close(min_sock) > 0);
	minet_deinit();
    /*close socket and deinitialize */

    return (ok ? 0 : -1);
}
