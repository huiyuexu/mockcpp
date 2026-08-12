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

#include <inttypes.h>
#include <string.h>

#include <mockcpp/JmpCode.h>
#include "AArch64PltResolver.h"
#include "JmpCodeArch.h"

MOCKCPP_NS_START

struct JmpCodeImpl
{
   ////////////////////////////////////////////////
   JmpCodeImpl(const void* from, const void* to)
      : m_codeSize(0)
      , m_patchAddress(GET_JMP_CODE_PATCH_ADDRESS(from))
      , m_patchModuleHandle(0)
   {
#if defined(__aarch64__) || defined(_M_ARM64)
      const AArch64PltResolution resolved =
         resolveAArch64PltAddress(from);
      m_patchAddress = const_cast<void*>(resolved.address);
      m_patchModuleHandle = resolved.moduleHandle;
      m_codeSize = buildAArch64JmpCode(m_code, m_patchAddress, to);
      if(m_patchModuleHandle != 0 &&
         (m_codeSize > resolved.availablePatchSize ||
          !isAArch64PatchRangeSafe(m_patchAddress, m_codeSize)))
      {
         releaseAArch64PltModule(m_patchModuleHandle);
         m_patchModuleHandle = 0;
         m_patchAddress = const_cast<void*>(resolved.fallbackAddress);
         m_codeSize = buildAArch64JmpCode(m_code, m_patchAddress, to);
      }
#elif defined(__arm__) || defined(_M_ARM)
      m_codeSize = buildArmJmpCode(m_code, from, to);
#else
      m_codeSize = sizeof(jmpCodeTemplate);
      ::memcpy(m_code, jmpCodeTemplate, sizeof(jmpCodeTemplate));
      SET_JMP_CODE(m_code, from, to);
#endif
   }

   ////////////////////////////////////////////////
   ~JmpCodeImpl()
   {
      releaseAArch64PltModule(m_patchModuleHandle);
   }

   ////////////////////////////////////////////////
   void*  getCodeData() const
   {
      return (void*) m_code;
   }

   ////////////////////////////////////////////////
   size_t getCodeSize() const
   {
      return m_codeSize;
   }

   ////////////////////////////////////////////////
   void* getPatchAddress() const
   {
      return m_patchAddress;
   }

   ////////////////////////////////////////////////

#if defined(__aarch64__) || defined(_M_ARM64) || \
    defined(__arm__) || defined(_M_ARM)
   unsigned char m_code[MAX_JMP_CODE_SIZE];
#else
   unsigned char m_code[sizeof(jmpCodeTemplate)];
#endif
   size_t m_codeSize;
   void* m_patchAddress;
   void* m_patchModuleHandle;
};

///////////////////////////////////////////////////
JmpCode::JmpCode(const void* from, const void* to)
   : This(new JmpCodeImpl(from, to))
{
}

///////////////////////////////////////////////////
JmpCode::~JmpCode()
{
   delete This;
}

///////////////////////////////////////////////////
void*
JmpCode::getCodeData() const
{
   return This->getCodeData();
}

///////////////////////////////////////////////////
size_t
JmpCode::getCodeSize() const
{
   return This->getCodeSize();
}

///////////////////////////////////////////////////
void*
JmpCode::getPatchAddress() const
{
   return This->getPatchAddress();
}

MOCKCPP_NS_END
