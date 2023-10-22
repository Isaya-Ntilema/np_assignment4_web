#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>

// Predefined static response headers
const char *const HTTP_OK_HEAD = "HTTP/1.1 200 OK\n";
const char *const HTTP_403 = "HTTP/1.1 403 Forbidden\nContent-Length: 166\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this webserver.\n</body></html>\n";
const char *const HTTP_404 = "HTTP/1.1 404 Not Found\nContent-Length: 146\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>404 Not Found</h1>\nThe requested resource was not found on the server.\n</body></html>\n";

const char *const CONT_TYPE_BASE = "Content-Type: ";
const char *const CONT_TYPE_HTML = "Content-Type: text/html\n\n";
const char *const CONTENT_LEN_BASE = "Content-Length: ";
const char *const SERVER = "FORK HTTP Server\n";
const char *const CONN_CLOSE = "Connection: close\n";

const char *const HTTP_DATE_RESP_FORMAT = "%a, %d %b %Y %H:%M:%S %Z";

// Define Size constants
#define DATESTAMP_LENGTH 30
#define BUFFER_SIZE 1024

const char *get_file_ext(const char *filename)
{
  const char *dot = strrchr(filename, '.'); // finds last occurance of .
  if (!dot || dot == filename)
    return "";
  return dot + 1;
}

int get_content_length(FILE *fp)
{
  int size = 0;
  fseek(fp, 0, SEEK_END);
  size = ftell(fp);
  rewind(fp);
  return size;
}

void getDate(char *dateStr)
{
  char buf[DATESTAMP_LENGTH];
  time_t now = time(0);
  struct tm tm = *(gmtime(&now));
  strftime(buf, sizeof(buf), HTTP_DATE_RESP_FORMAT, &tm);
  strcat(dateStr, buf);
  strcat(dateStr, "\n");
}

void *processRequest(void *newSock)
{
  int s = *((int *)newSock);
  char buffer[BUFFER_SIZE];

  memset(&buffer, 0, sizeof(buffer));
  
  // Get the request body from the socket
  if (recv(s, buffer, BUFFER_SIZE - 1, 0) == -1)
  {
    printf("Error handling incoming request \n");
    close(s);
    return NULL;
  }

  char *method = strtok(buffer, " ");
  char *path = strtok(NULL, " ");

  int r = strlen((const char *)path);
  int count = 0;
  for (int c = 0; c < r; c++)
  {
    if (path[c] == '/')
      count++;
  }
  
  if (count > 1)
  {
    printf("Only root directory is accepted!!");
    return 0;
  }

  // Format the response date *****
  char date[DATESTAMP_LENGTH + 6] = "Date: ";
  getDate(date);

  // Get content type from file extension

  char *ctype = NULL;

  //  // If filename given doesn't have an extension or the content type isn't supported. show error 403
  //  if (ctype == NULL)
  //  {
  //     write(s, HTTP_403, strlen(HTTP_403));
  //     close(s);
  //     return (NULL);
  //  }

  // Remove leading slash from path
  path++;

  // Try to open the requested file
  FILE *fp;
  fp = fopen(path, "rb");
  if (fp == NULL)
  {
    printf("file has problem");
    write(s, HTTP_404, strlen(HTTP_404));
    close(s);
    return (NULL);
  }
  else
  {
    //   printf("file opened successfully\n");
  }

  // The requested resource was found and opened. Get the content length of the file
  int contentLength;
  contentLength = get_content_length(fp);

  // Send headers and content
  write(s, HTTP_OK_HEAD, strlen(HTTP_OK_HEAD));
  // printf("SERVER %s", SERVER);
  write(s, SERVER, strlen(SERVER));

  // Send content length
  char contentLengthString[40];
  // sprintf(contentLengthString, "%s %d\n", CONTENT_LEN_BASE, contentLength);
  write(s, contentLengthString, strlen(contentLengthString));
  // printf("ScontentLengthString %s",contentLengthString);

  write(s, CONN_CLOSE, strlen(CONN_CLOSE));
  // printf(" CONN_CLOSE %s", CONN_CLOSE);

  write(s, date, strlen(date));
  // printf(" date %s", date);

  // Send the content type
  char contentType[80];
  sprintf(contentType, "%s %s\n\n", CONT_TYPE_BASE, ctype);
  write(s, contentType, strlen(contentType));
  // printf(" contentType %s", contentType);

  // Write each byte of the file to the socket
  int current_char = 0;
  do
  {
    current_char = fgetc(fp);
    write(s, &current_char, sizeof(char));

  } while (current_char != EOF);
  fclose(fp);
  close(s);
  // free(&s);
  // return NULL;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{

  int connfd;
  char sendBuff[1024];
  int sockfd; // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  // Check number of argumnets
  if (argc != 2)
  {
    fprintf(stderr, "usage: showip hostname\n");
    return 1;
  }

  // Seperating IP and port
  char delim[] = ":";
  char *Desthost = strtok(argv[1], delim);
  char *Destport = strtok(NULL, delim);

  /* Do magic chage string to int*/
  int port = atoi(Destport);
  printf("Host %s, and port %d.\n", Desthost, port);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // use my IP

  // Getting address information
  if ((rv = getaddrinfo(Desthost, Destport, &hints, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
                         p->ai_protocol)) == -1)
    {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(int)) == -1)
    {
      perror("setsockopt");
      exit(1);
    }
    // Binding start
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sockfd);
      perror("server: bind");
      exit(1);
    }

    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL)
  {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }
  if (listen(sockfd, 1000) == -1)
  {
    perror("listen");
    close(sockfd);
    exit(1);
  }

  memset(sendBuff, '0', sizeof(sendBuff));

  // printf("Entering loop....\r\n");
  while (1)
  {

    sin_size = sizeof(their_addr);
    //    printf("Accepting...\r\n");
    //    Accepting Connection
    connfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (connfd == -1)
    {
      perror("accept");
      continue;
    }

    // Getting peer ip address
    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s);

    //    printf("server: Connection  from %s\n", s);

    if (fork() == 0)
    {

      processRequest(&connfd);
      //  respond(slot);
      exit(0);
    }
    else
    {
      signal(SIGCHLD, SIG_IGN);
      close(connfd);
      
    }
  }

  close(sockfd);
  return 0;
}
