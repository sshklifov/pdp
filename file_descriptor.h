#pragma once

#include "check.h"

#include <unistd.h>

namespace pdp {

struct FileDescriptor {
  FileDescriptor(int descriptor) : fd(descriptor) {}

  FileDescriptor(FileDescriptor &&) = delete;
  FileDescriptor(const FileDescriptor &) = delete;

  void operator=(const FileDescriptor &) = delete;
  void operator=(FileDescriptor &&) = delete;

  ~FileDescriptor() { Check(close(fd), "close"); }

  void WaitForData(unsigned milliseconds);

 private:
  int fd;
};

}  // namespace pdp
