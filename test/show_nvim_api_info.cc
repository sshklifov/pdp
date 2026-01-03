#include "parser/expr.h"
#include "parser/rpc_parser.h"

#include <sys/wait.h>

using namespace pdp;

int main() {
  int fds[2];
  pdp_assert(pipe(fds) == 0);

  pid_t pid = fork();
  pdp_assert(pid >= 0);

  if (pid == 0) {
    // child: run nvim --api-info
    dup2(fds[1], STDOUT_FILENO);
    close(fds[0]);
    close(fds[1]);

    execlp("nvim", "nvim", "--api-info", nullptr);
    _exit(1);
  }

  // parent
  close(fds[1]);

  RpcPass pass(fds[0]);
  ExprView e(pass.Parse());

  // wait for nvim to exit
  int status = 0;
  waitpid(pid, &status, 0);
  pdp_assert(WIFEXITED(status));

  // ---- debug dump (THIS is why we did this) ----
  e.Print();
}
