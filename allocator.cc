#include "allocator.h"

#include "Mutex.h"
#include "external/emhash8.h"

#include <cstdlib>

namespace pdp {

void *BasicAllocator::AllocateRaw(size_t bytes) { return malloc(bytes); }

void *BasicAllocator::ReallocateRaw(void *ptr, size_t bytes) { return realloc(ptr, bytes); }

void BasicAllocator::DeallocateRaw(void *ptr, size_t _) { free(ptr); }

void *TracingAllocator::AllocateRaw(size_t bytes) {
#ifdef PDP_TRACE_ALLOCATIONS
  // TODO
#else
  return nullptr;
#endif
}

void *TracingAllocator::ReallocateRaw(void *ptr, size_t bytes) {
#ifdef PDP_TRACE_ALLOCATIONS
  // TODO
#else
  return nullptr;
#endif
}

void TracingAllocator::DeallocateRaw(void *ptr, size_t bytes) {
#ifdef PDP_TRACE_ALLOCATIONS
  // TODO
#endif
}

}  // namespace pdp
