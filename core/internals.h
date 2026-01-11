#pragma once

#define PDP_CONSTEXPR_EVALUATED() __builtin_is_constant_evaluated()

#define PDP_LIKELY(x) __builtin_expect(static_cast<bool>(x), true)
#define PDP_UNLIKELY(x) __builtin_expect(static_cast<bool>(x), false)

#define PDP_CLZ(x) __builtin_clz(x)
#define PDP_CLZLL(x) __builtin_clzll(x)

#define PDP_UNREACHABLE(msg) ::pdp::OnFatalError(__FILE__, __LINE__, msg)

#define PDP_ASSUME_ALIGNED(x, a) __builtin_assume_aligned(x, a)

namespace pdp {

using byte = unsigned char;

}  // namespace pdp
