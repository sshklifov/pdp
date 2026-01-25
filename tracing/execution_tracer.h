#pragma once

#include "core/internals.h"
#include "system/time_units.h"

#include <sys/poll.h>
#include <unistd.h>
#include <cstddef>

namespace pdp {

// TODO #ifdef PDP_DEBUG_BUILD

enum class RecordType { kRead, kFork, kWaitPid, kPoll, kTimeLess, kTimeNotLess, kTotal };

namespace impl {

struct _Recorder {
  _Recorder(const char *path);

  ~_Recorder();

  ssize_t RecordSyscallRead(int fd, void *buf, ssize_t ret);
  int RecordSyscallPoll(struct pollfd *poll_args, nfds_t n, int ret);

  pid_t RecordSyscallFork(pid_t child_pid);
  pid_t RecordSyscallWaitPid(pid_t pid, int status);

  bool RecordIsTimeLess(bool check_passed);

 private:
  void Reserve(size_t required_size);

  static void RecordEnum(RecordType type, byte *out);
  static void RecordShort(short value, byte *out);
  static void RecordInteger(int value, byte *out);

  int recording_fd;

  byte *scratch_buffer;
  size_t scratch_size;

  static constexpr size_t max_scratch_size = 1048576;
};

struct _Replayer {
  _Replayer(const char *path);
  ~_Replayer();

  bool IsEndOfStream() const;
  void PrintDiskUsage();

  ssize_t ReplaySyscallRead(int fd, void *buf, size_t size);
  int ReplaySyscallPoll(struct pollfd *poll_args, nfds_t n);

  pid_t ReplaySyscallFork();
  pid_t ReplaySyscallWaitPid(int *status);

  bool ReplayIsTimeLess();

 private:
  void CheckForEnd() const;

  static RecordType GetRecordType(const byte *in);
  static bool IsRecordType(RecordType expected_type, const byte *in);
  static short ReplayShort(const byte *out);
  static int ReplayInteger(const byte *in);

  int recording_fd;
  unsigned syscall_read_bytes;
  const byte *__restrict ptr;
  byte *__restrict orig_ptr;
  const byte *__restrict limit;
};

}  // namespace impl

enum class ExecMode { kNormal, kRecord, kReplay };

struct ExecutionTracer {
  ExecutionTracer();

  ~ExecutionTracer();
  void Stop();

  void StartRecording();
  void StartRecording(const char *path);
  void StopRecording();

  void StartReplaying(const char *path);
  void StartReplaying();
  void StopReplaying();

  void CheckForEndOfStream();

  bool IsReplaying() const;
  bool IsRecording() const;
  bool IsNormal() const;

  bool IsTimeLess(Milliseconds lhs, Milliseconds rhs);

  ssize_t SyscallRead(int fd, void *buf, size_t size);
  ssize_t SyscallWrite(int fd, const void *buf, size_t size);
  int SyscallPoll(struct pollfd *poll_args, nfds_t n, int timeout);

  pid_t SyscallWaitPid(int *status, int options);
  pid_t SyscallFork();

 private:
  impl::_Recorder *AsRecorder();
  impl::_Replayer *AsReplay();

  ExecMode mode;

  static constexpr size_t storage_size = 32;
  alignas(std::max_align_t) byte storage[storage_size];

  static_assert(sizeof(impl::_Recorder) <= storage_size);
  static_assert(sizeof(impl::_Replayer) <= storage_size);
};

extern ExecutionTracer g_recorder;

};  // namespace pdp
