#include "parser/expr.h"
#include "parser/rpc_parser.h"

#include <sys/wait.h>

using namespace pdp;

int main(int argc, char **argv) {
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

  // ---- debug dump ----
  StringBuilder msg;
  if (argc > 1) {
    auto functions = e["functions"];
    for (size_t i = 0; i < functions.Count(); ++i) {
      auto name = functions[i]["name"];
      if (name == argv[1]) {
        functions[i].ToJson(msg);
        break;
      }
    }
  } else {
    e.ToJson(msg);
  }

  msg.Append('\n');
  write(STDOUT_FILENO, msg.Data(), msg.Size());
  return 0;
}
