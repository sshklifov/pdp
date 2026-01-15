#pragma once

#include <cstddef>

namespace pdp {

#ifdef PDP_DEBUG_BUILD
void FramePointerBacktrace(void **destination, size_t size);
void PrintBacktrace(void **bt, size_t size);
#endif

};  // namespace pdp
