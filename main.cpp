#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <iostream>

using namespace std;

#define PORT_NUMBER 5001

//---------------------------------------------------------------------------
#define BUFFER_SIZE (4 * 1024)
size_t sendfile_rw(int fd_dst, int fd_src, size_t n)
{
  char *buffer[BUFFER_SIZE];
  size_t bytes_left = n;

  while (bytes_left > 0)
  {
    size_t block_size = (bytes_left < BUFFER_SIZE) ? bytes_left : BUFFER_SIZE;

    if (read(fd_src, buffer, block_size) < 0)
    {
      perror("error while reading");
      break;
    }

    if (write(fd_dst, buffer, block_size) < 0)
    {
      perror("error while writing");
      break;
    }

    bytes_left -= block_size;
  }

  return n - bytes_left;
}

//---------------------------------------------------------------------------
size_t sendfile_sendfile(int fd_dst, int fd_src, size_t n)
{
  size_t bytes_left = n;
  off_t offset = 0;

  while (bytes_left > 0)
  {
    size_t block_size = sendfile(fd_dst, fd_src, &offset, bytes_left);
    bytes_left -= block_size;
  }

  return n - bytes_left;
}

//---------------------------------------------------------------------------
#define MAX_SPLICE_SIZE (16 * 1024)
size_t sendfile_splice(int fd_dst, int fd_src, size_t n)
{
  int pfd[2];
  size_t bytes_left = n;
  // loff_t off1;
  // loff_t off2;

  // Create pipe
  if (pipe(pfd) < 0)
  {
    perror("pipe failed");
    return 0;
  }

  while (bytes_left > 0)
  {
    size_t block_size = (bytes_left < MAX_SPLICE_SIZE) ? bytes_left : MAX_SPLICE_SIZE;

    // Splice to fd_src -> pipe
    size_t splice_size = splice(fd_src, NULL, pfd[1], NULL, block_size, SPLICE_F_MOVE);

    if (splice_size == 0)
    {
      break;
    }

    // Splice pipe -> fd_dst
    splice(pfd[0], NULL, fd_dst, NULL, splice_size, SPLICE_F_MOVE);

    bytes_left -= splice_size;
  }

  return n - bytes_left;
}

//---------------------------------------------------------------------------
int client_start(const char *hostname)
{
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0)
  {
    perror("CClient::run: ERROR opening socket");
    return -1;
  }

  server = gethostbyname(hostname);

  if (server == NULL)
  {
    cout << "CClient::run: ERROR, no such host" << endl;
    return -1;
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(PORT_NUMBER);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("CClient::run: ERROR connecting");
    return -1;
  }

  return sockfd;
}

//---------------------------------------------------------------------------
int server_start()
{
  int sockfd, newsockfd;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0)
  {
    perror("ERROR opening socket");
    return -1;
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(PORT_NUMBER);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("ERROR on binding");
    return -1;
  }

  cout << "Waiting for clients..." << endl;

  ::listen(sockfd, 5);
  clilen = sizeof(cli_addr);
  newsockfd = ::accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

  if (newsockfd < 0)
  {
    perror("ERROR on accept");
    return -1;
  }

  cout << "Client connected" << endl;

  return newsockfd;
}

//---------------------------------------------------------------------------
void print_usage()
{
  cout << "Usage: netsplice <mode> receive fileout" << endl;
  cout << "       netsplice <mode> send localhost filein" << endl;
  cout << "       netsplice <mode> copy filein fileout" << endl;
  cout << endl;
  cout << "       <mode>:" << endl;
  cout << "         rw" << endl;
  cout << "         sendfile" << endl;
  cout << "         splice" << endl;
}

//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  int fd_filein;
  int fd_fileout;
  struct stat sb;
  uint64_t filesize;
  size_t transferred;

  cout << "netsplice - simple application for transferring files" << endl;

  if (argc < 4)
  {
    print_usage();
    return -1;
  }

  string sMode = argv[1];

  if ((sMode != "rw") && (sMode != "sendfile") && (sMode != "splice"))
  {
    cout << "invalid transfer <mode> selected, choose rw, sendfile or splice"
         << endl;

    print_usage();
    return -1;
  }

  string sAction = argv[2];
  if (sAction == "receive")
  {
    if (argc != 4)
    {
      print_usage();
      return -1;
    }

    fd_filein = server_start();

    if (fd_filein < 0)
    {
      cout << "Unable start socket server" << endl;
      return -1;
    }

    // Get the filesize from the client
    read(fd_filein, &filesize, sizeof(filesize));

    fd_fileout = open(argv[3], O_WRONLY | O_CREAT, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    if (fd_fileout < 0)
    {
      perror("Unable to open output file");
      return -1;
    }
  }
  else if (sAction == "send")
  {
    if (argc != 5)
    {
      print_usage();
      return -1;
    }

    fd_filein = open(argv[4], O_RDONLY);

    if (fd_filein < 0)
    {
      perror("Unable to open input file");
      return -1;
    }

    fstat(fd_filein, &sb);
    filesize = sb.st_size;
    cout << "filesize: " << filesize << endl;

    fd_fileout = client_start(argv[3]);

    if (fd_fileout < 0)
    {
      cout << "Unable start socket client" << endl;
      return -1;
    }

    // Tell the server what we are about to send
    write(fd_fileout, &filesize, sizeof(filesize));
  }
  else if (sAction == "copy")
  {
    if (argc != 5)
    {
      print_usage();
      return -1;
    }

    fd_filein = open(argv[3], O_RDONLY);

    if (fd_filein < 0)
    {
      perror("Unable to open input file");
      return -1;
    }

    fstat(fd_filein, &sb);
    filesize = sb.st_size;
    cout << "filesize: " << filesize << endl;

    fd_fileout = open(argv[4], O_WRONLY | O_CREAT, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    if (fd_fileout < 0)
    {
      perror("Unable to open output file");
      return -1;
    }
  }
  else
  {
    print_usage();
    return -1;
  }

  cout << "starting " << sMode << " " << sAction << " of "
       << (filesize / (1024 * 1024)) << "MiB" << endl;

  if (sMode == "rw")
  {
    transferred = sendfile_rw(fd_fileout, fd_filein, filesize);
  }
  else if (sMode == "sendfile")
  {
    transferred = sendfile_sendfile(fd_fileout, fd_filein, filesize);
  }
  else if (sMode == "splice")
  {
    transferred = sendfile_splice(fd_fileout, fd_filein, filesize);
  }

  cout << " - Done " << (transferred / (1024 * 1024)) << "MiB" << endl;

  close(fd_fileout);
  close(fd_filein);

  return 0;
}
