#include "backtrace.h"
#include "internals.h"
#include "log.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <limits.h>
#include <link.h>
#include <cstring>

namespace pdp {

void FramePointerBacktrace(void **destination, size_t size) {
  // Note: For a safer approach, use `pthread_attr_getstack` to obtain the stack start and discard
  // invalid backtraces.
  void **fp = (void **)PDP_FRAME_ADDRESS(0);
  for (size_t i = 0; i < size; ++i) {
    void **next_fp = (void **)*fp;
    void *pc = fp[1];
    destination[i] = pc;
    if (next_fp <= fp) {
      return;
    }
    fp = next_fp;
  }
}

static void PrintHexadecimal(uint64_t bits) {
  // TODO comment how this needs to be copy pasted.
  const auto max_digits = sizeof(void *) * 2 + 2;
  char buffer[max_digits];

  const uint64_t digits = pdp::CountDigits16(bits);

  const char *lookup = "0123456789abcdef";

  char *__restrict__ store = buffer + digits + 1;
  do {
    *store = lookup[bits & 0xf];
    --store;
    bits >>= 4;
  } while (bits != 0);

  *store = 'x';
  --store;
  *store = '0';
  LogUnformatted(StringSlice(buffer, digits + 2));
}

static void PrintSymbolNoNewline(const void *pc) {
  char resolved[PATH_MAX];

  Dl_info info;
  memset(&info, 0, sizeof(info));
  struct link_map *extra_info = NULL;

  int ret = dladdr1(pc, &info, (void **)&extra_info, RTLD_DL_LINKMAP);
  if (ret) {
    if (info.dli_sname) {
      int status = -1;
      char *demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
      if (demangled) {
        LogUnformatted(demangled);
        free(demangled);
      } else {
        LogUnformatted(info.dli_sname);
      }
      return;
    } else if (info.dli_fname) {
      if (realpath(info.dli_fname, resolved)) {
        LogUnformatted(resolved);
      } else {
        LogUnformatted("./");
        LogUnformatted(info.dli_fname);
      }
      char *base_addr = extra_info ? (char *)extra_info->l_addr : (char *)info.dli_fbase;
      ptrdiff_t offset = 0;
      if (base_addr <= pc) {
        offset = (char *)pc - base_addr;
        LogUnformatted("(+");
      } else {
        offset = base_addr - (char *)pc;
        LogUnformatted("(-");
      }
      PrintHexadecimal(BitCast<uint64_t>(offset));
      LogUnformatted(")");
      return;
    }
  }
  PrintHexadecimal(BitCast<uint64_t>(pc));
}

void PrintBacktrace(void **bt, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (!bt[i]) {
      return;
    }
    PrintSymbolNoNewline(bt[i]);
    LogUnformatted("\n");
  }
}

}  // namespace pdp
