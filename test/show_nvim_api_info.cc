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

  ByteStream stream(fds[0]);
  RpcChunkArrayPass pass(stream);
  StrongTypedView e(pass.Parse());

  // wait for nvim to exit
  int status = 0;
  waitpid(pid, &status, 0);
  pdp_assert(WIFEXITED(status));

  // ---- debug dump ----
  StringBuilder msg;
  if (argc > 1) {
    auto functions = e["functions"];
    for (uint32_t i = 0; i < functions.Count(); ++i) {
      auto name = functions[i]["name"];
      auto str = name.AsString();
      if (memmem(str.Data(), str.Size(), argv[1], strlen(argv[1]))) {
        functions[i].ToJson(msg);
        msg.Append('\n');
      }
#if 0
      auto ret = functions[i]["return_type"];
      if (ret) {
        msg.AppendFormat("{}: ", str);
        ret.ToJson(msg);
        msg.Append('\n');
      }
#endif
    }
  } else {
    e.ToJson(msg);
    msg.Append('\n');
  }

  write(STDOUT_FILENO, msg.Data(), msg.Size());
  return 0;
}
