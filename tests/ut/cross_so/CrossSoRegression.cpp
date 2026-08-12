#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include <pthread.h>
#include <sys/socket.h>
#include <dlfcn.h>

#include "CrossSoTarget.h"

USING_MOCKCPP_NS

namespace
{

typedef int (*CrossSoFunction)(int);

bool hasInstruction(const void* address,
                    unsigned char byte0,
                    unsigned char byte1)
{
   const unsigned char* instruction =
      static_cast<const unsigned char*>(address);
   return instruction[0] == byte0 && instruction[1] == byte1 &&
          instruction[2] == 0x03U && instruction[3] == 0xd5U;
}

bool belongsToMainExecutable(const void* address, const void* mainAddress)
{
   Dl_info symbolInfo;
   Dl_info mainInfo;
   return ::dladdr(address, &symbolInfo) != 0 &&
          ::dladdr(mainAddress, &mainInfo) != 0 &&
          symbolInfo.dli_fbase == mainInfo.dli_fbase;
}

}

int main()
{
   if(!belongsToMainExecutable(reinterpret_cast<const void*>(&socket),
                               reinterpret_cast<const void*>(&main)) ||
      !belongsToMainExecutable(reinterpret_cast<const void*>(&listen),
                               reinterpret_cast<const void*>(&main)) ||
      !belongsToMainExecutable(
         reinterpret_cast<const void*>(&pthread_rwlock_rdlock),
         reinterpret_cast<const void*>(&main)) ||
      !belongsToMainExecutable(
         reinterpret_cast<const void*>(&pthread_rwlock_destroy),
         reinterpret_cast<const void*>(&main)))
   {
      return 9;
   }

   const void* executableAddress =
      reinterpret_cast<const void*>(&mockcpp_cross_so_target);
   const bool executablePltHasBti =
      hasInstruction(executableAddress, 0x5fU, 0x24U);
   CrossSoFunction executableFunction =
      reinterpret_cast<CrossSoFunction>(
         const_cast<void*>(executableAddress));

   // The regression is only meaningful when the executable exposes the
   // imported function through its canonical PLT while the shared object
   // uses the real local definition.
   if(executableAddress == mockcpp_cross_so_target_address())
   {
      return 10;
   }

   if(mockcpp_cross_so_caller(5) != 6)
   {
      return 11;
   }

   MOCKER(mockcpp_cross_so_target)
      .stubs()
      .will(returnValue(77));

   if(mockcpp_cross_so_caller(5) != 77 || executableFunction(5) != 77 ||
      (executablePltHasBti &&
       !hasInstruction(executableAddress, 0x5fU, 0x24U)))
   {
      GlobalMockObject::reset();
      return 12;
   }

   GlobalMockObject::reset();
   if(mockcpp_cross_so_caller(5) != 6)
   {
      return 13;
   }

   const void* shortExecutableAddress =
      reinterpret_cast<const void*>(&mockcpp_cross_so_short_target);
   const bool shortPltHasBti =
      hasInstruction(shortExecutableAddress, 0x5fU, 0x24U);
   CrossSoFunction shortExecutableFunction =
      reinterpret_cast<CrossSoFunction>(
         const_cast<void*>(shortExecutableAddress));
   if(shortExecutableAddress == mockcpp_cross_so_short_target_address() ||
      mockcpp_cross_so_short_caller(5) != 6 ||
      mockcpp_cross_so_short_neighbor() != 123)
   {
      return 20;
   }

   MOCKER(mockcpp_cross_so_short_target)
      .stubs()
      .will(returnValue(78));

   // A far-away short target uses a 4-byte branch to a nearby trampoline.  It
   // must be hooked without overwriting the following function.
   if(mockcpp_cross_so_short_caller(5) != 78 ||
      shortExecutableFunction(5) != 78 ||
      mockcpp_cross_so_short_neighbor() != 123 ||
      (shortPltHasBti &&
       !hasInstruction(shortExecutableAddress, 0x5fU, 0x24U)))
   {
      GlobalMockObject::reset();
      return 21;
   }

   GlobalMockObject::reset();
   if(mockcpp_cross_so_short_caller(5) != 6 ||
      mockcpp_cross_so_short_neighbor() != 123)
   {
      return 22;
   }

   const void* btiExecutableAddress =
      reinterpret_cast<const void*>(&mockcpp_cross_so_bti_target);
   const bool btiPltHasBti =
      hasInstruction(btiExecutableAddress, 0x5fU, 0x24U);
   CrossSoFunction btiExecutableFunction =
      reinterpret_cast<CrossSoFunction>(
         const_cast<void*>(btiExecutableAddress));
   const void* btiRealAddress = mockcpp_cross_so_bti_target_address();
   if(btiExecutableAddress == mockcpp_cross_so_bti_target_address() ||
      mockcpp_cross_so_bti_caller(5) != 6 ||
      !hasInstruction(btiRealAddress, 0x5fU, 0x24U))
   {
      return 30;
   }

   MOCKER(mockcpp_cross_so_bti_target)
      .stubs()
      .will(returnValue(79));

   if(mockcpp_cross_so_bti_caller(5) != 79 ||
      btiExecutableFunction(5) != 79 ||
      !hasInstruction(btiRealAddress, 0x5fU, 0x24U) ||
      (btiPltHasBti &&
       !hasInstruction(btiExecutableAddress, 0x5fU, 0x24U)))
   {
      GlobalMockObject::reset();
      return 31;
   }

   GlobalMockObject::reset();
   if(mockcpp_cross_so_bti_caller(5) != 6)
   {
      return 32;
   }

   const void* pacExecutableAddress =
      reinterpret_cast<const void*>(&mockcpp_cross_so_pac_target);
   const bool pacPltHasBti =
      hasInstruction(pacExecutableAddress, 0x5fU, 0x24U);
   CrossSoFunction pacExecutableFunction =
      reinterpret_cast<CrossSoFunction>(
         const_cast<void*>(pacExecutableAddress));
   const void* pacRealAddress = mockcpp_cross_so_pac_target_address();
   if(pacExecutableAddress == mockcpp_cross_so_pac_target_address() ||
      mockcpp_cross_so_pac_caller(5) != 6 ||
      mockcpp_cross_so_pac_neighbor() != 124 ||
      !hasInstruction(pacRealAddress, 0x3fU, 0x23U))
   {
      return 40;
   }

   MOCKER(mockcpp_cross_so_pac_target)
      .stubs()
      .will(returnValue(80));

   // PACIASP/PACIBSP cannot be retained before a normal mock stub and cannot
   // be replaced on a BTI-guarded page.  The safe behavior is to leave the
   // shared-object-local entry untouched.
   if(mockcpp_cross_so_pac_caller(5) != 6 ||
      pacExecutableFunction(5) != 80 ||
      mockcpp_cross_so_pac_neighbor() != 124 ||
      !hasInstruction(pacRealAddress, 0x3fU, 0x23U) ||
      (pacPltHasBti &&
       !hasInstruction(pacExecutableAddress, 0x5fU, 0x24U)))
   {
      GlobalMockObject::reset();
      return 41;
   }

   GlobalMockObject::reset();
   if(mockcpp_cross_so_pac_caller(5) != 6 ||
      mockcpp_cross_so_pac_neighbor() != 124)
   {
      return 42;
   }

   if(mockcpp_cross_so_socket_caller() != -1)
   {
      return 49;
   }

   MOCKER(socket).stubs().will(returnValue(501));
   if(mockcpp_cross_so_socket_caller() != 501)
   {
      GlobalMockObject::reset();
      return 50;
   }
   GlobalMockObject::reset();
   if(mockcpp_cross_so_socket_caller() != -1)
   {
      return 53;
   }

   if(mockcpp_cross_so_listen_caller() != -1)
   {
      return 54;
   }
   MOCKER(listen).stubs().will(returnValue(502));
   if(mockcpp_cross_so_listen_caller() != 502)
   {
      GlobalMockObject::reset();
      return 51;
   }
   GlobalMockObject::reset();
   if(mockcpp_cross_so_listen_caller() != -1)
   {
      return 55;
   }

   if(mockcpp_cross_so_rwlock_caller() != 0)
   {
      return 56;
   }
   MOCKER(pthread_rwlock_rdlock).stubs().will(returnValue(503));
   if(mockcpp_cross_so_rwlock_caller() != 503)
   {
      GlobalMockObject::reset();
      return 52;
   }

   GlobalMockObject::reset();
   if(mockcpp_cross_so_rwlock_caller() != 0)
   {
      return 57;
   }

   if(mockcpp_cross_so_rwlock_destroy_caller() != 0)
   {
      return 58;
   }
   MOCKER(pthread_rwlock_destroy).stubs().will(returnValue(504));
   if(mockcpp_cross_so_rwlock_destroy_caller() != 504)
   {
      GlobalMockObject::reset();
      return 59;
   }

   GlobalMockObject::reset();
   return mockcpp_cross_so_rwlock_destroy_caller() == 0 ? 0 : 60;
}
