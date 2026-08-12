#include "CrossSoTarget.h"

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_target(int value)
{
#if defined(__aarch64__)
   // Keep enough replaceable instructions for mockcpp's far-jump sequence.
   __asm__ __volatile__("nop\n\tnop\n\tnop\n\tnop");
#endif
   return value + 1;
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_caller(int value)
{
   return mockcpp_cross_so_target(value);
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_target_address()
{
   return reinterpret_cast<const void*>(&mockcpp_cross_so_target);
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_short_caller(int value)
{
   return mockcpp_cross_so_short_target(value);
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_short_target_address()
{
   return reinterpret_cast<const void*>(&mockcpp_cross_so_short_target);
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_bti_caller(int value)
{
   return mockcpp_cross_so_bti_target(value);
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_bti_target_address()
{
   return reinterpret_cast<const void*>(&mockcpp_cross_so_bti_target);
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_pac_caller(int value)
{
   return mockcpp_cross_so_pac_target(value);
}

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_pac_target_address()
{
   return reinterpret_cast<const void*>(&mockcpp_cross_so_pac_target);
}
