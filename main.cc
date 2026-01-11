#include "application/vim_controller.h"
#include "core/log.h"

#include <fcntl.h>
#include <sys/prctl.h>

int alt_main() {
  unsigned char msg[] = {0x94, 0x00, 0x01, 0xac, 'n', 'v', 'i', 'm', '_', 'c', 'o', 'm', 'm', 'a',
                         'n',  'd',  0x91, 0xaa, 'e', 'c', 'h', 'o', 'm', ' ', '"', 'h', 'i', '"'};

  int flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
  int ret = fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);
  pdp::CheckAndTerminate(ret, "fcntl");

  write(STDOUT_FILENO, msg, sizeof(msg));
  sleep(5);
  return 0;
}

int main() {
  // return alt_main();

#ifdef PDP_DEBUG_BUILD
  prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY);
#endif

  pdp::LogRedirectRAII redirect_raii("/home/stef/Downloads/output.txt");

  pdp_info("Starting vim controller");
  pdp::VimController vim(pdp::DuplicateForThisProcess(STDOUT_FILENO),
                         pdp::DuplicateForThisProcess(STDIN_FILENO));

  pdp_info("Sending hello world to buffer 0");
  vim.ShowNormal("Hello world!");
  sleep(30);

  return 0;
}
