/***
   mockcpp is a C/C++ mock framework.
   Copyright [2008] [Darwin Yuan <darwin.yuan@gmail.com>]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
***/
#ifndef __MOCKCPP_AARCH64_PLT_RESOLVER_H__
#define __MOCKCPP_AARCH64_PLT_RESOLVER_H__

#include <stddef.h>

#include <mockcpp/mockcpp.h>

MOCKCPP_NS_START

struct AArch64PltResolution
{
   AArch64PltResolution(const void* resolvedAddress,
                        void* handle,
                        size_t patchSize = 0,
                        const void* safeFallbackAddress = 0)
      : address(resolvedAddress)
      , moduleHandle(handle)
      , availablePatchSize(patchSize)
      , fallbackAddress(safeFallbackAddress == 0
                           ? resolvedAddress : safeFallbackAddress)
   {
   }

   const void* address;
   void* moduleHandle;
   size_t availablePatchSize;
   const void* fallbackAddress;
};

AArch64PltResolution resolveAArch64PltAddress(const void* address);
bool isAArch64PatchRangeSafe(const void* address, size_t size);
void releaseAArch64PltModule(void* moduleHandle);

MOCKCPP_NS_END

#endif
