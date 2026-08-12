#ifndef __MOCKCPP_CROSS_SO_TARGET_H__
#define __MOCKCPP_CROSS_SO_TARGET_H__

#if defined(__GNUC__)
#  define MOCKCPP_CROSS_SO_API __attribute__((visibility("default")))
#  define MOCKCPP_CROSS_SO_NOINLINE __attribute__((noinline))
#else
#  define MOCKCPP_CROSS_SO_API
#  define MOCKCPP_CROSS_SO_NOINLINE
#endif

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_target(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_caller(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_target_address();

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_short_target(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_short_caller(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_short_neighbor();

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_short_target_address();

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_bti_target(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_bti_caller(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_bti_target_address();

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_pac_target(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_pac_caller(int value);

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
int mockcpp_cross_so_pac_neighbor();

extern "C" MOCKCPP_CROSS_SO_API MOCKCPP_CROSS_SO_NOINLINE
const void* mockcpp_cross_so_pac_target_address();

#endif
