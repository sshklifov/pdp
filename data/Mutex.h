#pragma once

#include <pthread.h>

namespace pdp {

struct Mutex {
  Mutex() {
#ifdef PDP_CHECK_MUTEX
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&mutex, &attr);
    pthread_mutexattr_destroy(&attr);
#else
    pthread_mutex_init(&mutex, NULL);
#endif
  }

  Mutex(const Mutex &) = delete;

  Mutex(Mutex &&) = delete;

  void operator=(const Mutex &) = delete;

  void operator=(Mutex &&) = delete;

  void Lock() { pthread_mutex_lock(&mutex); }

  void Unlock() { pthread_mutex_unlock(&mutex); }

 private:
  pthread_mutex_t mutex;
};

struct ScopedLock {
  ScopedLock(Mutex &m) : mutex(m) { m.Lock(); }

  ~ScopedLock() { mutex.Unlock(); }

 private:
  Mutex &mutex;
};

}  // namespace pdp
