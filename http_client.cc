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

    /* make socket */
    int min_sock = minet_socket(SOCK_STREAM);

    /* get host IP address  */
    struct hostent *hostentry = gethostbyname(server_name);

    /* Hint: use gethostbyname() */
    struct sockaddr_in hostsockaddr;
    hostsockaddr.sin_family = AF_INET;
    hostsockaddr.sin_port = htons(server_port);
    //hostsockaddr.sin_addr.s_addr = inet_addr(hostentry->h_addr);
    memcpy((void *)&(hostsockaddr.sin_addr.s_addr),(void *)hostentry->h_addr, sizeof(hostentry->h_addr));
    /* set address */ 

    /* connect to the server socket */
    if (minet_connect(min_sock, &hostsockaddr) < 0) {
      //TODO: Handle error!!!!
      fprintf(stderr, "Failed at connect");
      exit(-1);
    }
    std::cout << "Connected to the yaw" << std::endl;

    /* send request message */
    sprintf(req, "GET %s HTTP/1.0\r\n\r\n", server_path);

    //std::cout << "Request is " << req << std::endl;

    if (minet_write(min_sock, req, path_len) < path_len) {
      //TODO: handle error!
      std::cout << "Oh no, wheres the yaw?" << std::endl;
      exit(-1);
    }

    std::cout << "Wrote some yaw" << std::endl;    

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(min_sock, &readfds);
    /* wait till socket can be read. */

    if(minet_select(min_sock + 1, &readfds, NULL, NULL, NULL) < 1) {
      //TODO: error handling
      fprintf(stderr, "got nothing from select!");
    }
    /* Hint: use select(), and ignore timeout for now. */

    char * buffer = (char*)calloc(BUFSIZE + 1, sizeof(char));

    /* first read loop -- read headers */

    int buff_len = minet_read(min_sock, buffer, BUFSIZE);
    if (buff_len < 1) {
          //TODO: Error handling!
    }
    
   // buffer[(int)buff_len] = '\0'; //Null terminate string
        //DO some regex stuff here?

    std::string response(buffer);
    //Parse up to end of header
    std::size_t head_end = response.find("\r\n\r\n");
    // std::cout << buffer << std::endl;
    // std::cout << response << std::endl;

    std::string header = response.substr(9, head_end);
    /* examine return code */
    //Skip "HTTP/1.0"
    //remove the '\0'

    if (header.substr(0, 3).compare("200") == 0) {
      //The request was a success!
    }
    else {
      //Something went wrong!
    }
    std::cout << header << std::endl; //Print header

    // Normal reply has return code 200

    /* print first part of response: header, error code, etc. */

    //Read from end of header, skipping \r\n\r\n
	
	FD_ZERO(&readfds);
    FD_SET(min_sock, &readfds);
	memset(buffer, '\0', sideof(char) * 1025);
	
    while(minet_select(min_sock + 1, &readfds, NULL, NULL, NULL) > 0){
		if (!FD_ISSET(min_sock, &readfds)){
			//TODO: handle error!!!
		}
		
		buff_len = minet_read(min_sock, buffer, BUFSIZE);
		
		if (buff_len < 1) {
          //TODO: Error handling!
		}
		std::cout << buffer << std::endl;
		FD_ZERO(&readfds);
		FD_SET(min_sock, &readfds);
		memset(buffer, '\0', sideof(char) * 1025);
	}
    std::string body = response.substr(head_end);
    std::cout << body << std::endl;

    /* second read loop -- print out the rest of the response: real web content */

    ok = minet_close(min_sock) && minet_deinit();
    /*close socket and deinitialize */

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
