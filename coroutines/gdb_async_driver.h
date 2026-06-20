#pragma once

#include "data/callback.h"
#include "drivers/gdb_driver.h"
#include "parser/expr.h"
#include "system/child_reaper.h"
#include "system/poll_table.h"

namespace pdp {

struct GdbAsyncDriver {
  using StreamCallback = Callback<StringSlice>;
  using ErrorCallback = Callback<StringSlice>;
  using AsyncCallback = Callback<GdbAsyncKind, UniquePtr<ExprBase>>;
  using ResultCallback = Callback<GdbResultKind, UniquePtr<ExprBase>>;

  GdbAsyncDriver(ChildReaper &reaper);

  template <typename Fun>
  void SetStreamCallback(Fun &&f) {
    stream_cb.Assign(std::forward<Fun>(f));
  }

  template <typename Fun>
  void SetAsyncCallback(Fun &&f) {
    async_cb.Assign(std::forward<Fun>(f));
  }

  template <typename Fun>
  void SetResultCallback(Fun &&f) {
    result_cb.Assign(std::forward<Fun>(f));
  }

  template <typename Fun>
  void SetErrorCallback(Fun &&f) {
    error_cb.Assign(std::forward<Fun>(f));
  }

  void RegisterForPoll(PollTable &table);
  void OnPollResults(PollTable &table);

  template <typename... Args>
  void Send(const StringSlice &fmt, Args &&...args) {
    auto packed_args = MakePackedArgs(std::forward<Args>(args)...);
    gdb_driver.Send(token, fmt, packed_args.slots, packed_args.type_bits);
    ++token;
  }

 private:
  void DrainRecords();
  void DrainErrors();

  void HandleStream(const StringSlice &msg);
  void HandleAsync(GdbAsyncKind kind, UniquePtr<ExprBase> expr);
  void HandleResult(GdbResultKind kind, UniquePtr<ExprBase> expr);

  uint32_t token;
  GdbDriver gdb_driver;

  StreamCallback stream_cb;
  AsyncCallback async_cb;
  ResultCallback result_cb;
  ErrorCallback error_cb;
};

}  // namespace pdp
