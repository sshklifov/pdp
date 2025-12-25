#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"

int main() {
  info("We are in the game {}!", 10);

  int in[2];   // parent -> gdb (stdin)
  int out[2];  // gdb -> parent (stdout)
  int err[2];  // gdb -> parent (stderr)

  pipe(in);
  pipe(out);
  pipe(err);

  pid_t pid = fork();
  if (pid == 0) {
    // child: GDB
    dup2(in[0], STDIN_FILENO);
    dup2(out[1], STDOUT_FILENO);
    dup2(err[1], STDERR_FILENO);

    close(in[1]);
    close(out[0]);
    close(err[0]);

    execlp("gdb", "gdb", "--interpreter=mi2", nullptr);
    perror("exec gdb");
    _exit(1);
  }

  // parent
  close(in[0]);
  close(out[1]);
  close(err[1]);

  // send a command
  const char *cmd = "-gdb-version\n";
  write(in[1], cmd, strlen(cmd));

  // read response
  char buf[4096];
  ssize_t n = read(out[0], buf, sizeof(buf));
  write(STDOUT_FILENO, buf, n);

  return 0;
}
