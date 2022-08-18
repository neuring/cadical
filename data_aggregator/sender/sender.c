#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//char *socket_path = "./socket";
char *socket_path = "\0hidden";

typedef struct Clause {
    int *literals;
    int length;
} Clause;

void send_clause(int fd, Clause *clause, int lbd) {
    size_t size_in_bytes = clause->length * sizeof(int);
    size_t bytes_written = write(fd, clause->literals, size_in_bytes);

    if (bytes_written != size_in_bytes) {
      if (bytes_written > 0) fprintf(stderr,"partial write\n");
      else {
        perror("write error\n");
        exit(-1);
      }
    }

    int remaining_data[2] = {0, lbd};

    size_in_bytes = sizeof(remaining_data);
    bytes_written = write(fd, remaining_data, size_in_bytes);
    if (bytes_written != size_in_bytes) {
      if (bytes_written > 0) fprintf(stderr,"partial write\n");
      else {
        perror("write error\n");
        exit(-1);
      }
    }
}

int main(int argc, char *argv[]) {
  struct sockaddr_un addr;
  char buf[100];
  int fd,rc;
  printf("start\n");

  if (argc > 1) socket_path=argv[1];
  printf("Attempt to connect to socket %s\n", socket_path);

  if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("socket error\n");
    exit(-1);
  }

  printf("got filedescriptor\n");

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (*socket_path == '\0') {
    *addr.sun_path = '\0';
    strncpy(addr.sun_path+1, socket_path+1, sizeof(addr.sun_path)-2);
  } else {
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  }

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("connect error\n");
    exit(-1);
  }

  printf("Connected\n");

  while( (rc=read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
    int literals[] = {1,2,3,4};
    Clause cls = { literals, sizeof(literals) / sizeof(int)};
    static int next_lbd = 2;
    send_clause(fd, &cls, next_lbd);
    next_lbd += 1;
    printf("Sent data\n");
  }

  return 0;
}