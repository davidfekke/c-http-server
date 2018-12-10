// http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#simpleserver
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

#define PORT "3000" // The port users will connect to
#define BACKLOG 10 // how many pending connections queue will hold

void sigchld_handler (int s) {
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6
void* get_in_addr (struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*) sa)->sin_addr);
  } else {
    return &(((struct sockaddr_in6*) sa)->sin6_addr);
  }
}

int main () {
  int sockfd, new_fd; // listen on sock_fd, new connection on new_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr; // connector's address
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;

  hints = (struct addrinfo) { 0 }; // initialize to 0
  hints = (struct addrinfo) {
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_PASSIVE // use my IP
  };

  if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (int)) == -1) {
      perror("setsockopt");
      return 1;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server:bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "server: failed to bind\n");
    return 2;
  }

  freeaddrinfo(servinfo); // all done with this struct

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    return 1;
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    return 1;
  }

  puts("server: waiting for connections...");

  while (1) { // main accept loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr*) &their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr*) &their_addr), s, sizeof s);
    printf("server: got connection from %s\n", s);

    if (!fork()) { // this is the child process
      close(sockfd); // child doesn't need listener
      const char* const msg = "Hello Single threaded world!\n";
      const char* const fmt_header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %ld\r\n"
        "\r\n"
        "%s";
      char buf [200];
      snprintf(buf, 200, fmt_header, strlen(msg), msg);

      if (send(new_fd, buf, strlen(buf), 0) == -1) {
        perror("send");
      }
      close(new_fd);
      return 0;
    }
    close(new_fd); // parent doesn't need this
  }
}

