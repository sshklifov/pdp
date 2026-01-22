#pragma once

#include "data/non_copyable.h"
#include "time_units.h"

#include <cstddef>

namespace pdp {

int DuplicateForThisProcess(int fd);
void SetNonBlocking(int fd);

struct FileDescriptor : public NonCopyableNonMovable {
  FileDescriptor();
  FileDescriptor(int descriptor);
  ~FileDescriptor();

  bool IsValid() const;
  int GetDescriptor() const;
  void SetDescriptor(int init_fd);

 protected:
  bool WaitForEvents(int events, Milliseconds timeout);

  int fd;
};

struct InputDescriptor : public FileDescriptor {
  using FileDescriptor::FileDescriptor;

  bool WaitForInput(Milliseconds timeout);

  size_t ReadAtLeast(void *buf, size_t required_bytes, size_t free_bytes, Milliseconds timeout);
  bool ReadExactly(void *buf, size_t size, Milliseconds timeout);

  size_t ReadAvailable(void *buf, size_t max_bytes);

  size_t ReadOnce(void *buf, size_t size);

 private:
};

struct OutputDescriptor : public FileDescriptor {
  using FileDescriptor::FileDescriptor;

  bool WaitForOutput(Milliseconds timeout);
  bool WriteExactly(const void *buf, size_t bytes, Milliseconds timeout);

  size_t WriteOnce(const void *buf, size_t size);
};

}  // namespace pdp
