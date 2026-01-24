#include "execution_tracer.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/check.h"
#include "core/log.h"

namespace pdp {

ExecutionTracer g_recorder;

namespace impl {

_Recorder::_Recorder(const char *path) {
  recording_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  CheckFatal(recording_fd, "Failed to create recording file!");

  scratch_size = 1024;
  scratch_buffer = (byte *)malloc(scratch_size);
  pdp_assert(scratch_buffer);
}

_Recorder::~_Recorder() {
  free(scratch_buffer);
  Check(close(recording_fd), "Recorder::close");
}

void _Recorder::RecordEnum(RecordType type, byte *out) { *out = static_cast<byte>(type); }

void _Recorder::RecordShort(short value, byte *out) {
  *out++ = (static_cast<byte>((value >> 8) & 0xFF));
  *out++ = (static_cast<byte>(value & 0xFF));
}

void _Recorder::RecordInteger(int value, byte *out) {
  *out++ = (static_cast<byte>((value >> 24) & 0xFF));
  *out++ = (static_cast<byte>((value >> 16) & 0xFF));
  *out++ = (static_cast<byte>((value >> 8) & 0xFF));
  *out++ = (static_cast<byte>(value & 0xFF));
}

void _Recorder::Reserve(size_t required_size) {
  if (PDP_UNLIKELY(required_size > scratch_size)) {
    pdp_assert(required_size <= max_scratch_size);
    while (scratch_size < required_size) {
      scratch_size *= 2;
    }
    scratch_buffer = (byte *)realloc(scratch_buffer, scratch_size);
    pdp_assert(scratch_buffer);
  }
}

ssize_t _Recorder::RecordSyscallRead(int target_fd, void *buf, ssize_t ret) {
  if (PDP_UNLIKELY(ret > INT_MAX || ret < INT_MIN)) {
    PDP_UNREACHABLE("Cannot record integer: overflow");
  }

  Reserve(ret + 9);
  RecordEnum(RecordType::kRead, scratch_buffer);
  RecordInteger(static_cast<int>(ret), scratch_buffer + 1);
  RecordInteger(target_fd, scratch_buffer + 5);

  if (ret <= 0) {
    WriteFully(recording_fd, scratch_buffer, 9);
  } else {
    memcpy(scratch_buffer + 9, buf, static_cast<size_t>(ret));
    WriteFully(recording_fd, scratch_buffer, ret + 9);
  }
  return ret;
}

int _Recorder::RecordSyscallPoll(struct pollfd *poll_args, nfds_t n, int ret) {
  if (PDP_UNLIKELY(n > INT_MAX)) {
    PDP_UNREACHABLE("Cannot record poll: overflow");
  }
  auto required_size = 9 + n * 2;
  Reserve(required_size);

  RecordEnum(RecordType::kPoll, scratch_buffer);
  RecordInteger(ret, scratch_buffer + 1);
  if (ret <= 0) {
    WriteFully(recording_fd, scratch_buffer, 5);
  } else {
    int num_fds = static_cast<int>(n);
    RecordInteger(num_fds, scratch_buffer + 5);
    byte *pos = scratch_buffer + 9;
    for (nfds_t i = 0; i < n; ++i) {
      RecordShort(poll_args[i].revents, pos);
      pos += 2;
    }
    WriteFully(recording_fd, scratch_buffer, 9 + n * 2);
  }
  return ret;
}

pid_t _Recorder::RecordSyscallFork(pid_t child_pid) {
  pdp_assert(child_pid > 0);
  if (PDP_UNLIKELY(child_pid > INT_MAX)) {
    PDP_UNREACHABLE("Cannot record pid: overflow");
  }
  RecordEnum(RecordType::kFork, scratch_buffer);
  RecordInteger(child_pid, scratch_buffer + 1);
  WriteFully(recording_fd, scratch_buffer, 5);
  return child_pid;
}

pid_t _Recorder::RecordSyscallWaitPid(pid_t pid, int status) {
  if (PDP_UNLIKELY(pid > INT_MAX || pid < INT_MIN)) {
    PDP_UNREACHABLE("Cannot record pid: overflow");
  }

  RecordEnum(RecordType::kWaitPid, scratch_buffer);
  RecordInteger(pid, scratch_buffer + 1);
  if (pid > 0) {
    RecordInteger(status, scratch_buffer + 5);
    WriteFully(recording_fd, scratch_buffer, 9);
  } else {
    WriteFully(recording_fd, scratch_buffer, 5);
  }
  return pid;
}

bool _Recorder::RecordIsTimeLess(bool is_less) {
  if (is_less) {
    RecordEnum(RecordType::kTimeLess, scratch_buffer);
  } else {
    RecordEnum(RecordType::kTimeNotLess, scratch_buffer);
  }
  WriteFully(recording_fd, scratch_buffer, 1);
  return is_less;
}

_Replayer::_Replayer(const char *path) {
  recording_fd = open(path, O_RDONLY | O_CLOEXEC);
  CheckFatal(recording_fd, "Failed to open recording file!");

  struct stat st;
  CheckFatal(fstat(recording_fd, &st), "Replay::fstat");
  pdp_assert(st.st_size > 0);

  size_t total_bytes = static_cast<size_t>(st.st_size);
  orig_ptr = (byte *)mmap(NULL, total_bytes, PROT_READ, MAP_PRIVATE, recording_fd, 0);
  CheckFatal(orig_ptr, "Replay::mmap");
  madvise(orig_ptr, total_bytes, MADV_SEQUENTIAL);

  ptr = orig_ptr;
  limit = orig_ptr + total_bytes;
}

_Replayer::~_Replayer() {
  const size_t len = limit - orig_ptr;
  munmap(orig_ptr, len);
  close(recording_fd);
}

short _Replayer::ReplayShort(const byte *in) {
  return (static_cast<short>(in[0]) << 8) | (static_cast<short>(in[1]));
}

int _Replayer::ReplayInteger(const byte *in) {
  return (static_cast<int>(in[0]) << 24) | (static_cast<int>(in[1]) << 16) |
         (static_cast<int>(in[2]) << 8) | (static_cast<int>(in[3]));
}

RecordType _Replayer::GetRecordType(const byte *in) {
  pdp_assert(*in < static_cast<int>(RecordType::kTotal));
  return static_cast<RecordType>(*in);
}

bool _Replayer::IsRecordType(RecordType expected_type, const byte *in) {
  return GetRecordType(in) == expected_type;
}

ssize_t _Replayer::ReplaySyscallRead(int fd, void *buf, size_t size) {
  CheckForEnd();

  if (PDP_UNLIKELY(!IsRecordType(RecordType::kRead, ptr))) {
    pdp_critical("Record byte: {}", MakeHex(*ptr));
    PDP_UNREACHABLE("Corrupted recording detected, read failed");
  }

  pdp_assert(limit - ptr >= 9);
  ssize_t ret = static_cast<ssize_t>(ReplayInteger(ptr + 1));
  int check_fd = ReplayInteger(ptr + 5);

  if (PDP_UNLIKELY(fd != check_fd)) {
    pdp_critical("Record fd: {}", check_fd);
    PDP_UNREACHABLE("Corrupted recording detected, read failed");
  }

  ptr += 9;
  if (ret > 0) {
    pdp_assert(static_cast<size_t>(ret) <= size);
    pdp_assert(limit - ptr >= ret);
    memcpy(buf, ptr, static_cast<size_t>(ret));
    ptr += ret;
  }
  return ret;
}

int _Replayer::ReplaySyscallPoll(struct pollfd *poll_args, nfds_t n) {
  CheckForEnd();

  const bool match = IsRecordType(RecordType::kPoll, ptr);
  if (PDP_UNLIKELY(!match)) {
    pdp_critical("Unexpected byte: {}", MakeHex(*ptr));
    PDP_UNREACHABLE("Corrupted recording detected, poll failed");
  }

  pdp_assert(limit - ptr >= 5);
  int ret = ReplayInteger(ptr + 1);
  ptr += 5;
  if (PDP_LIKELY(ret > 0)) {
    int m = ReplayInteger(ptr);
    pdp_assert(static_cast<int>(n) == m);
    ptr += 4;
    pdp_assert(limit - ptr >= m * 2);
    for (int i = 0; i < m; ++i) {
      poll_args[i].revents = ReplayShort(ptr);
      ptr += 2;
    }
  }
  return ret;
}

pid_t _Replayer::ReplaySyscallFork() {
  CheckForEnd();

  const bool match = IsRecordType(RecordType::kFork, ptr);
  if (PDP_UNLIKELY(!match)) {
    pdp_critical("Unexpected byte: {}", MakeHex(*ptr));
    PDP_UNREACHABLE("Corrupted recording detected, fork failed");
  }

  pdp_assert(limit - ptr >= 5);
  pid_t child_pid = static_cast<pid_t>(ReplayInteger(ptr + 1));
  ptr += 5;
  return child_pid;
}

pid_t _Replayer::ReplaySyscallWaitPid(int *status) {
  CheckForEnd();

  const bool match = IsRecordType(RecordType::kWaitPid, ptr);
  if (PDP_UNLIKELY(!match)) {
    pdp_critical("Unexpected byte: {}", MakeHex(*ptr));
    PDP_UNREACHABLE("Corrupted recording detected, waitpid failed");
  }

  pdp_assert(limit - ptr >= 5);
  pid_t pid = static_cast<pid_t>(ReplayInteger(ptr + 1));
  ptr += 5;
  if (pid > 0) {
    pdp_assert(limit - ptr >= 4);
    *status = ReplayInteger(ptr);
    ptr += 4;
  }
  return pid;
}

bool _Replayer::ReplayIsTimeLess() {
  CheckForEnd();

  auto rec = GetRecordType(ptr);
  ++ptr;
  if (rec == RecordType::kTimeLess) {
    return true;
  } else if (rec == RecordType::kTimeNotLess) {
    return false;
  }

  PDP_UNREACHABLE("Corrupted recording detected, time check failed");
}

void _Replayer::CheckForEnd() const {
  if (IsEndOfStream()) {
    PDP_TRAP();
  }
}

bool _Replayer::IsEndOfStream() const {
  pdp_assert(ptr <= limit);
  return ptr == limit;
}

}  // namespace impl

ExecutionTracer::ExecutionTracer() : mode(ExecMode::kNormal) {}

ExecutionTracer::~ExecutionTracer() { Stop(); }

void ExecutionTracer::Stop() {
  if (mode == ExecMode::kRecord) {
    StopRecording();
  } else if (mode == ExecMode::kRecord) {
    StopReplaying();
  }
  mode = ExecMode::kNormal;
}

void ExecutionTracer::CheckForEndOfStream() {
  if (mode == ExecMode::kReplay) {
    if (AsReplay()->IsEndOfStream()) {
      pdp_info("Replay EOS reached.");
    } else {
      pdp_warning("Replay still in progress, stopping...");
    }
  }
}

impl::_Recorder *ExecutionTracer::AsRecorder() {
  pdp_assert(mode == ExecMode::kRecord);
  return reinterpret_cast<impl::_Recorder *>(storage);
}

impl::_Replayer *ExecutionTracer::AsReplay() {
  pdp_assert(mode == ExecMode::kReplay);
  return reinterpret_cast<impl::_Replayer *>(storage);
}

void ExecutionTracer::StartRecording(const char *path) {
  pdp_assert(mode == ExecMode::kNormal);
  mode = ExecMode::kRecord;

  new (AsRecorder()) impl::_Recorder(path);
  pdp_info("Recording to {}...", StringSlice(path));
}

void ExecutionTracer::StartRecording() { StartRecording(PDP_RECORDER_PATH); }

void ExecutionTracer::StopRecording() {
  pdp_assert(mode == ExecMode::kRecord);
  AsRecorder()->~_Recorder();
  mode = ExecMode::kNormal;
}

void ExecutionTracer::StartReplaying(const char *path) {
  pdp_assert(mode == ExecMode::kNormal);
  mode = ExecMode::kReplay;
  new (AsReplay()) impl::_Replayer(path);
  pdp_info("Replaying {}...", StringSlice(path));
}

void ExecutionTracer::StartReplaying() { StartReplaying(PDP_RECORDER_PATH); }

void ExecutionTracer::StopReplaying() {
  pdp_assert(mode == ExecMode::kReplay);
  AsReplay()->~_Replayer();
  mode = ExecMode::kNormal;
}

bool ExecutionTracer::IsReplaying() const { return mode == ExecMode::kReplay; }

bool ExecutionTracer::IsRecording() const { return mode == ExecMode::kRecord; }

bool ExecutionTracer::IsNormal() const { return mode == ExecMode::kNormal; }

bool ExecutionTracer::IsTimeLess(Milliseconds lhs, Milliseconds rhs) {
  const bool is_less = lhs < rhs;
  switch (mode) {
    case ExecMode::kNormal:
      return is_less;

    case ExecMode::kRecord:
      return AsRecorder()->RecordIsTimeLess(is_less);

    case ExecMode::kReplay:
      return AsReplay()->ReplayIsTimeLess();
  }
  pdp_assert(false);
}

ssize_t ExecutionTracer::SyscallRead(int fd, void *buf, size_t size) {
  ssize_t ret = 0;
  switch (mode) {
    case ExecMode::kNormal:
      return read(fd, buf, size);

    case ExecMode::kRecord:
      ret = read(fd, buf, size);
      return AsRecorder()->RecordSyscallRead(fd, buf, ret);

    case ExecMode::kReplay:
      return AsReplay()->ReplaySyscallRead(fd, buf, size);
  }
  pdp_assert(false);
}

ssize_t ExecutionTracer::SyscallWrite(int fd, const void *buf, size_t size) {
  switch (mode) {
    case ExecMode::kNormal:
    case ExecMode::kRecord:
      return write(fd, buf, size);
    case ExecMode::kReplay:
      return static_cast<ssize_t>(size);
  }
  pdp_assert(false);
}

pid_t ExecutionTracer::SyscallFork() {
  pid_t child_pid = 0;
  switch (mode) {
    case ExecMode::kNormal:
      return fork();

    case ExecMode::kRecord:
      child_pid = fork();
      if (child_pid > 0) {
        return AsRecorder()->RecordSyscallFork(child_pid);
      } else {
        return child_pid;
      }

    case ExecMode::kReplay:
      return AsReplay()->ReplaySyscallFork();
  }
  pdp_assert(false);
}

pid_t ExecutionTracer::SyscallWaitPid(int *status, int options) {
  pid_t child_pid = 0;
  switch (mode) {
    case ExecMode::kNormal:
      return waitpid(-1, status, options);

    case ExecMode::kRecord:
      child_pid = waitpid(-1, status, options);
      return AsRecorder()->RecordSyscallWaitPid(child_pid, *status);

    case ExecMode::kReplay:
      return AsReplay()->ReplaySyscallWaitPid(status);
  }
  pdp_assert(false);
}

int ExecutionTracer::SyscallPoll(struct pollfd *poll_args, nfds_t n, int timeout) {
  int ret = 0;
  switch (mode) {
    case ExecMode::kNormal:
      return poll(poll_args, n, timeout);

    case ExecMode::kRecord:
      ret = poll(poll_args, n, timeout);
      AsRecorder()->RecordSyscallPoll(poll_args, n, ret);
      return ret;

    case ExecMode::kReplay:
      return AsReplay()->ReplaySyscallPoll(poll_args, n);
  }
  pdp_assert(false);
}

};  // namespace pdp
