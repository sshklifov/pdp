#pragma once

#define PDP_CONSTEXPR_EVALUATED() __builtin_is_constant_evaluated()

#define PDP_LIKELY(x) __builtin_expect(static_cast<bool>(x), true)
#define PDP_UNLIKELY(x) __builtin_expect(static_cast<bool>(x), false)
