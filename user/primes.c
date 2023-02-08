#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void sieve_primes(int prime, int from, int to) {
  uint8 byte;
  while (read(from, &byte, 1)) {
    if (byte % prime) {
      write(to, &byte, 1);
    }
  }
}

int main(int argc, char *argv[]) {
  int from;
  int to;
  {
    int p[2];
    pipe(p);
    from = p[0];
    to = p[1];
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork\n");
    exit(1);
  }

  if (pid) {
    close(from);
    for (uint8 i = 2; i <= 35; ++i) {
      write(to, &i, 1);
    }
    close(to);
    if (wait(0) < 0) {
      fprintf(2, "wait\n");
      exit(1);
    }
    exit(0);
  }

  close(to);
  for (;;) {
    uint8 byte;
    if (read(from, &byte, 1) != 1) {
      break;
    }
    int prime = byte;
    printf("prime %d\n", prime);

    int p[2];
    pipe(p);
    pid = fork();
    if (pid < 0) {
      fprintf(2, "fork\n");
      exit(1);
    }
    if (pid == 0) {
      from = p[0];
      close(p[1]);
      continue;
    }

    to = p[1];
    close(p[0]);

    sieve_primes(prime, from, to);

    close(from);
    close(to);
    if (wait(0) < 0) {
      fprintf(2, "wait\n");
      exit(1);
    }
  }

  exit(0);
}
