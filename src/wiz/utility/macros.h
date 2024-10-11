#ifndef WIZ_UTILITY_MACROS_H
#define WIZ_UTILITY_MACROS_H

#ifdef _MSC_VER
#define WIZ_FORCE_INLINE __forceinline
#define WIZ_MAYBE_UNUSED_FORCE_INLINE(x) __forceinline x
#elif defined(__clang__) || defined(__GNUC__)
#define WIZ_FORCE_INLINE inline __attribute__((always_inline))
#define WIZ_MAYBE_UNUSED_FORCE_INLINE(x) __attribute__((always_inline)) __attribute__((unused)) inline x
#include <stdint.h>
#else
#define WIZ_FORCE_INLINE inline
#define WIZ_MAYBE_UNUSED_FORCE_INLINE(x) inline x
#endif

#endif
