#pragma once

#include "data/non_copyable.h"
#include "time_units.h"

#include <cstddef>

namespace pdp {

struct FileDescriptor : public NonCopyableNonMovable {
  FileDescriptor();
  FileDescriptor(int descriptor);
  ~FileDescriptor();

  bool IsValid() const;
  int Value() const;
  void SetValue(int init_fd);

 protected:
  void SetNonBlocking();

  bool WaitForEvents(int events, Milliseconds timeout);

  int fd;
};

struct InputDescriptor : public FileDescriptor {
  using FileDescriptor::FileDescriptor;

  bool WaitForInput(Milliseconds timeout);

  size_t ReadAtLeast(void *buf, size_t required_bytes, size_t free_bytes, Milliseconds timeout);
  bool ReadExactly(void *buf, size_t size, Milliseconds timeout);

 private:
  size_t Read(void *buf, size_t size);
};

struct OutputDescriptor : public FileDescriptor {
  using FileDescriptor::FileDescriptor;

  bool WaitForOutput(Milliseconds timeout);
  bool WriteExactly(void *buf, size_t bytes, Milliseconds timeout);

 private:
  size_t Write(void *buf, size_t size);
};

}  // namespace pdp
