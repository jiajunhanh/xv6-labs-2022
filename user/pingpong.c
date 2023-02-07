#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  int pipe_parent_to_child[2];
  int pipe_child_to_parent[2];
  pipe(pipe_parent_to_child);
  pipe(pipe_child_to_parent);

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork\n");
    exit(1);
  }

  uint8 byte = 0;
  if (pid > 0) {
    write(pipe_parent_to_child[1], &byte, 1);
    read(pipe_child_to_parent[0], &byte, 1);
    printf("%d: received pong\n", getpid());
  } else {
    read(pipe_parent_to_child[0], &byte, 1);
    printf("%d: received ping\n", getpid());
    write(pipe_child_to_parent[1], &byte, 1);
  }

  exit(0);
}
