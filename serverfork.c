#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>

// Client request
extern char *method, 
    *uri,            // "/index.html" things before '?'
    *qs,             // "a=1&b=2" things after  '?'
    *prot,           // "HTTP/1.1"
    *payload;        // for POST

extern int payload_size;

// Server control functions
void serve_forever(const char *PORT);

char *request_header(const char *name);


typedef struct {
  char *name, *value;
} header_t;
static header_t reqhdr[17] = {{"\0", "\0"}};
header_t *request_headers(void);


#define MAX_CONNECTIONS 100000
#define BUF_SIZE 65535
#define QUEUE_SIZE 1000000

static int listenfd;
int *connfd;
static void start_server(const char *);
static void respond(int);

static char *buf;

// Client request
char *method, // "GET" or "POST"
    *uri,     // "/index.html" things before '?'
    *qs,      // "a=1&b=2" things after  '?'
    *prot,    // "HTTP/1.1"
    *payload; // for POST

int payload_size;


#define CHUNK_SIZE 1024 // read 1024 bytes at a time

// Public directory settings
#define ROOT_DIR "./"
#define INDEX_HTML "/index.html"
#define NOT_FOUND_HTML "/404.html"

// get all request headers
header_t *request_headers(void) 
{ 
    return reqhdr; 
}

void route();

// Response
#define RESPONSE_PROTOCOL "HTTP/1.1"

#define HTTP_200 printf("%s 200 OK\n\n", RESPONSE_PROTOCOL)
#define HTTP_201 printf("%s 201 Created\n\n", RESPONSE_PROTOCOL)
#define HTTP_404 printf("%s 404 Not found\n\n", RESPONSE_PROTOCOL)
#define HTTP_500 printf("%s 500 Internal Server Error\n\n", RESPONSE_PROTOCOL)

// some interesting macro for `route()`
#define ROUTE_START() if (0) {
#define ROUTE(METHOD, URI)                                                     \
  }                                                                            \
  else if (strcmp(URI, uri) == 0 && strcmp(METHOD, method) == 0) {
#define GET(URI) ROUTE("GET", URI)
#define POST(URI) ROUTE("POST", URI)
#define ROUTE_END()                                                            \
  }                                                                            \
  else HTTP_500;

// get request header by name
char *request_header(const char *name) 
{
  header_t *h = reqhdr;
  while (h->name) 
  {
    if (strcmp(h->name, name) == 0)
      return h->value;
    h++;
  }
  return NULL;
}

// Handle escape characters (%xx)
static void uri_unescape(char *uri) {
  char chr = 0;
  char *src = uri;
  char *dst = uri;

  // Skip inital non encoded character
  while (*src && !isspace((int)(*src)) && (*src != '%'))
    src++;

  // Replace encoded characters with corresponding code.
  dst = src;
  while (*src && !isspace((int)(*src))) {
    if (*src == '+')
      chr = ' ';
    else if ((*src == '%') && src[1] && src[2]) {
      src++;
      chr = ((*src & 0x0F) + 9 * (*src > '9')) * 16;
      src++;
      chr += ((*src & 0x0F) + 9 * (*src > '9'));
    } else
      chr = *src;
    *dst++ = chr;
    src++;
  }
  *dst = '\0';
}

//check filename available
int file_exists(const char *file_name)
{
  struct stat buffer;
  int exists;
  exists = (stat(file_name, &buffer) == 0);
  return exists;
}

//read the file content
int read_file(const char *file_name) 
{
  char buf[CHUNK_SIZE];
  FILE *file;
  size_t nread;
  int err = 1;

  file = fopen(file_name, "r");

  if (file) 
  {
    while ((nread = fread(buf, 1, sizeof buf, file)) > 0)
      fwrite(buf, 1, nread, stdout);

    err = ferror(file);
    fclose(file);
  }
  return err;
}


void route() 
{
  ROUTE_START()

  GET("/") 
  {
    char index_html[20];
    sprintf(index_html, "%s%s", ROOT_DIR, INDEX_HTML);

    HTTP_200;
    if (file_exists(index_html))
    {
        read_file(index_html);
    } 
    else 
    {
        printf("Hello! You are using %s\n\n", request_header("User-Agent"));
    }
  }

  GET("/test") 
  {
    HTTP_200;
    printf("List of request headers:\n\n");
    header_t *h = request_headers();
    while (h->name) 
    {
      printf("%s: %s\n", h->name, h->value);
      h++;
    }
  }

  POST("/")
  {
    HTTP_201;
    printf("Wow, seems that you POSTed %d bytes.\n", payload_size);
    printf("Fetch the data using `payload` variable.\n");
    if (payload_size > 0)
      printf("Request body: %s", payload);
  }

  GET(uri) 
  {
    char file_name[255];
    sprintf(file_name, "%s%s", ROOT_DIR, uri);

    if (file_exists(file_name))
    {
      HTTP_200;
      read_file(file_name);
    } 
    else 
    {
      HTTP_404;
      sprintf(file_name, "%s%s", ROOT_DIR, NOT_FOUND_HTML);
      if (file_exists(file_name))
        read_file(file_name);
    }
  }

  ROUTE_END()
}

// client connection
void client_handler(int index) 
{
    int rcvd;
    buf = malloc(BUF_SIZE);
    rcvd = recv(connfd[index], buf, BUF_SIZE, 0);

    if (rcvd < 0)
        printf("ERROR Message receive error \n");
    else if (rcvd == 0) 
        printf("ERROR Client disconnected upexpectedly.\n");
    else
    {
        buf[rcvd] = '\0';

        method = strtok(buf, " \t\r\n");
        uri = strtok(NULL, " \t");
        prot = strtok(NULL, " \t\r\n");

        uri_unescape(uri);

        printf("\x1b[32m + [%s] %s\x1b[0m\n", method, uri);

        qs = strchr(uri, '?');

        if (qs)
            *qs++ = '\0'; // split URI
        else
            qs = uri - 1; // use an empty string

        header_t *h = reqhdr;
        char *t, *t2;
        while (h < reqhdr + 16) 
        {
            char *key, *val;
            key = strtok(NULL, "\r\n: \t");
            if (!key)
                break;
            val = strtok(NULL, "\r\n");
            while (*val && *val == ' ')
            val++;

            h->name = key;
            h->value = val;
            h++;
            printf("[H] %s: %s\n", key, val);
            t = val + 1 + strlen(val);
            if (t[1] == '\r' && t[2] == '\n')
            break;
        }

    t = strtok(NULL, "\r\n");
    t2 = request_header("Content-Length"); // and the related header if there is
    payload = t;
    payload_size = t2 ? atol(t2) : (rcvd - (t - buf));

    // bind clientfd to stdout, making it easier to write
    int clientfd = connfd[index];
    dup2(clientfd, STDOUT_FILENO);
    close(clientfd);

    // call router
    route();

    // tidy up
    fflush(stdout);
    shutdown(STDOUT_FILENO, SHUT_WR);
    close(STDOUT_FILENO);
  }

  free(buf);
}


int main(int argc, char **argv)
{
	//check the commandline argurments 
	if(argc != 2)
	{
		printf("ERROR Please enter the  IP: port\n");
		exit(0);
	}
	//get the commnadline values
	char delim[]=":";
 	char *Desthost=strtok(argv[1],delim);
  	char *Destport=strtok(NULL,delim);
	int port=atoi(Destport);
	printf("Host %s, and port %d.\n",Desthost,port);

	// assign IP, PORT
	struct addrinfo hints;
	memset(&hints,0,sizeof(hints));
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_protocol=0;
	hints.ai_flags=AI_ADDRCONFIG;
	struct addrinfo* res=0;
	int err=getaddrinfo(Desthost,Destport,&hints,&res);
	if (err!=0) 
	{
    		perror("ERROR Failed to resolve remote socket address\n");
    		exit(0);
	}
	//Initialize variables
	int sockfd;
	int option = 1;
    struct sockaddr_in client;    
    int index =0;
    int c;
  
	// socket create and verification (server only support IPV4 and TCP )
	sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
	if(sockfd<=0)
	{
		printf("ERROR Server socket creation failed\n");
		exit(0);
	}
	else
		printf("Socket successfully created\n");

	if(setsockopt(sockfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0)
	{
		printf("ERROR setsockopt failed\n");
    	exit(0);
	}
	// Binding socket to server IP
	if ((bind(sockfd, res->ai_addr,res->ai_addrlen)) != 0) 
	{
		printf("ERROR Server socket bind failed\n");
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");

    // create shared memory for client slot array
    connfd = mmap(NULL, sizeof(*connfd) * MAX_CONNECTIONS,PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    // Setting all elements to -1: signifies there is no client connected
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++)
    connfd[i] = -1;

    // Ignore SIGCHLD to avoid zombie threads
    signal(SIGCHLD, SIG_IGN);

	// Server is ready to listen and verification
	if ((listen(sockfd,0)) != 0) 
	{
		printf("Only 10 clients can connect with the server, Listen failed / Server Rejected...\n");
		exit(0);
	}
	else
		printf("Web Server listening on Port:%d\n",port);

      while(1) 
      {
        connfd[index] = accept(sockfd,(struct sockaddr *)&client,(socklen_t*)&c);
        if (connfd[index]<0) 
        {
            printf("ERROR in accept client \n");
            break;
        }
        else
        {
            printf("New client accpeted \n");
            if (fork() == 0) 
            {
                close(sockfd);
                client_handler(index);
                close(connfd[index]);
                connfd[index] = -1;
                exit(0);
            } 
            else 
            {
                close(connfd[index]);
            }
        }
            while (connfd[index] != -1)
            index = (index + 1) % MAX_CONNECTIONS;
      }

    return 0;

}
