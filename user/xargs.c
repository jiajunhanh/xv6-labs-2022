#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_ARGS 10
#define BUFFER_SIZE 100

static int isspace(char ch) {
  return ch == ' ' || ch == '\n' || ch == '\t' || ch == '\v' || ch == '\f'
      || ch == '\r';
}

static char *get_arg(char **ptr) {
  while (isspace(**ptr)) {
    ++(*ptr);
  }
  if (**ptr == 0) {
    return 0;
  }

  char *ret = *ptr;
  while (!isspace(**ptr) && **ptr != 0) {
    // skip to the end of the arg
    ++(*ptr);
  }
  return ret;
}

static uint read_line(char *buffer, uint buffer_size) {
  memset(buffer, 0, buffer_size);
  gets(buffer, (int) buffer_size);
  return strlen(buffer);
}

static int parse_args(char *buffer, char **argv, int max_args) {
  int argc;
  for (argc = 0; argv[argc] != 0; ++argc);

  char *ptr = buffer;
  while ((argv[argc] = get_arg(&ptr)) != 0) {
    ++argc;
    if (argc >= max_args) {
      fprintf(2, "xargs: too many args\n");
      return -1;
    }
    if (*ptr != 0) {
      // terminate args with '\0'
      *ptr = 0;
      ++ptr;
    }
  }
  argv[argc] = 0;

  return argc;
}

int main(int argc, char *argv[]) {
  char *argv_exec[MAX_ARGS];
  char buffer[BUFFER_SIZE];

  if (argc - 1 >= MAX_ARGS) {
    fprintf(2, "xargs: too many args\n");
    exit(0);
  }

  for (int i = 1; i < argc; ++i) {
    argv_exec[i - 1] = argv[i];
  }

  while (read_line(buffer, sizeof(buffer))) {
    argv_exec[argc - 1] = 0;
    if (parse_args(buffer, argv_exec, MAX_ARGS) < 0) {
      continue;
    }
    int pid = fork();
    if (pid < 0) {
      fprintf(2, "fork\n");
      exit(1);
    } else if (pid > 0) {
      wait(0);
      continue;
    }
    exec(argv_exec[0], argv_exec);
    fprintf(2, "exec\n");
    exit(1);
  }

  exit(0);
}
