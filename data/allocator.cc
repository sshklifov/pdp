#include "allocator.h"

#include <cstdlib>

namespace pdp {

void *BasicAllocator::AllocateRaw(size_t bytes) { return malloc(bytes); }

void *BasicAllocator::ReallocateRaw(void *ptr, size_t new_bytes) { return realloc(ptr, new_bytes); }

void BasicAllocator::DeallocateRaw(void *ptr) { free(ptr); }

void *TracingAllocator::AllocateRaw(size_t bytes) {
#ifdef PDP_TRACE_ALLOCATIONS
  // TODO
#else
  return nullptr;
#endif
}

void *TracingAllocator::ReallocateRaw(void *ptr, size_t new_bytes) {
#ifdef PDP_TRACE_ALLOCATIONS
  // TODO
#else
  return nullptr;
#endif
}

void TracingAllocator::DeallocateRaw(void *ptr) {
#ifdef PDP_TRACE_ALLOCATIONS
  // TODO
#endif
}

}  // namespace pdp
