#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int sock);

int main(int argc, char * argv[]) {
    struct sockaddr_in netaddr;    //struct for IP address and port
    int bindStatus;            //holds status of bind
    int listenStatus;        //holds status of listen

    int server_port = -1;
    int rc          =  0;
    int sock        = -1;           //var for accept socket
    int sock2       = -1;          //var for connection socket

    /* parse command line args */
    if (argc != 3) {
        fprintf(stderr, "usage: http_server1 k|u port\n");
        exit(-1);
    }

    server_port = atoi(argv[2]);

    if (server_port < 1500) {
        fprintf(stderr, "INVALID PORT NUMBER: %d; can't be < 1500\n", server_port);
        exit(-1);
    }

    /* initialize and make socket */
    if (*(argv[1]) == 'k') {
      //kernel socket, initialize for kernel
      minet_init(MINET_KERNEL);
    } else if (*(argv[1]) ==  'u') {
      //user socket, initialize for user
      minet_init(MINET_USER);
    } else {
      //invalid option, exit with error
      fprintf(stderr, "First option arg must be a \"k\" or a \"u\"\n");
      exit(-1);
    }

    //creates new TCP socket for accept
    sock = minet_socket(SOCK_STREAM);
    if (sock < 0) {
        minet_perror("Error creating accept socket");
        minet_deinit();
        exit(-1);
    }


    /* set server address*/
    memset(&netaddr, 0, sizeof(netaddr)); //initializes struct to 0 in memory
    netaddr.sin_family = AF_INET;       //using IPv4
    netaddr.sin_addr.s_addr = htonl(INADDR_ANY); //sets IP in network byte order
    netaddr.sin_port = htons(server_port);    //sets port in network byte order

    /* bind listening socket */
    bindStatus = minet_bind(sock, &netaddr);
    if (bindStatus < 0) {
        minet_perror("Error binding socket");
        minet_close(sock);
        minet_deinit();
        exit(-1);
    }

    /* start listening */
    listenStatus = minet_listen(sock, 10); //allows queue of 10 connections
    if (listenStatus < 0) {
        minet_perror("Error listening");
        minet_close(sock);
        minet_deinit();
        exit(-1);
    }


    /* connection handling loop: wait to accept connection */

    while (1) {
        /* handle connections */
        //accept connection from client
        sock2 = minet_accept(sock, NULL);    //address info not needed
        if (sock2 < 0) {
            minet_perror("Error accepting connection");
            continue;           //continues looping for new connection
        }
        rc = handle_connection(sock2);
        if (rc < 0) {
            fprintf(stderr, "Error handling connection\n", );
       }
    }
    minet_close(sock);      //exited loop, closing accept socket
    fprintf(stderr, "Server exited unexpectedly\n");
    minet_deinit();         //shutdown Minet stack
    return 0;               //exits program
}

int handle_connection(int sock) {
    bool ok = false;
    char buf[BUFSIZE];
    char filename[FILENAMESIZE];
    int recvBytes = 0;              //holds number of bytes received
    int contentLength = 0;          //holds length of file
    int writeStatus = 0;

    const char * ok_response_f = "HTTP/1.0 200 OK\r\n"	\
	"Content-type: text/plain\r\n"			\
	"Content-length: %d \r\n\r\n";

    const char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"	\
	"Content-type: text/html\r\n\r\n"			\
	"<html><body bgColor=black text=white>\n"		\
	"<h2>404 FILE NOT FOUND</h2>\n"
	"</body></html>\n";

    /* first read loop -- get request and headers*/

    std::string request;     //String to hold entire request
    std::string curr;       //String to hold current buffered data
    std::size_t matched;     //holds pos of search for end of header
    bool found = false;     //flag for if header found or not
    recvBytes = minet_read(sock, buf, BUFSIZE);
    //loops while still data to be read from request and no header
    while (recvBytes > 0) {
        curr = buf;         //stores most recently read in string
        request += curr;    //appends to total request
        memset(buf, 0, BUFSIZE);    //resets buffer for next read
        //Tab and new line indicate end of header, need to find
        matched = request.find("\r\n\r\n"); //holds either pos or npos
        if (matched != std::string::npos) {   //sequence found
            request = request.substr(0, found); //store header
            found = true;       //read was a success
            break;          //no more need to loop
        }
        recvBytes = minet_read(sock, buf, BUFSIZE);   //read more into buffer
    }

    if (!found) {
        minet_perror("Error in reading request");
        return -1;          //indicates unsuccessful
    } else {
        /* parse request to get file name */
        /* Assumption: this is a GET request and filename contains no spaces*/
        //Format: GET file HTTP
        //pos of HTTP is equal to length of GET + 2 spaces + filename
        matched = request.find("HTTP");
        if (matched >= 6) {      //at least long enough for 1 char filename
            //stores string from beginning of filename at pos 4,
            //to end of name found by subtracting 5 from total length
            //before "HTTP"
            std::string fileTemp = request.substr(4, (matched - 5));
            strncpy(filename, fileTemp.c_str(), sizeof(filename));
            /* try opening the file */
            FILE *fp = fopen(filename, "r");
            if (fp != NULL) {
                ok = true;          //file exists
            }
            /* send response */
            if (ok) {
                   /* send headers */
                   fseek(fp, 0, SEEK_END);      //go to EOF
                   contentLength = ftell(fp);   //store length of file
                   char ok_response_full [strlen(ok_response_f) + 1];
                   //stores OK response with content length using format string
                   sprintf(ok_response_full, ok_response_f, contentLength);
                   writeStatus = minet_write(sock, ok_response_full, strlen(ok_response_full));
                   if (writeStatus < 0) {
                       minet_perror("Error sending headers");
                       ok = false;
                   }
                      /* send file */
                   fseek(fp, 0, SEEK_SET);      //reset pos in file
                   memset(buf, 0, BUFSIZE);     //reset buffer

                   while ((fread(buf, sizeof(char), BUFSIZE, fp)) > 0) {
                       writeStatus = minet_write(sock, buf, BUFSIZE);
                       if (writeStatus < 0) {
                           minet_perror("Error sending file");
                           ok = false;
                           break;
                       }
                       memset(buf, 0, BUFSIZE);
                   }
                   fclose(fp);

            } else {
              // send error response
              ok = true;    //ok if error message successfully sent
              writeStatus = minet_write(sock, (char*)notok_response, strlen(notok_response));
              if (writeStatus < 0) {
                  minet_perror("Error sending headers");
                  ok = false;
              }
            }
        }
    }

    /* close socket and free space */
    minet_close(sock);

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
