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

#if defined(__aarch64__) && defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <mockcpp/JmpCode.h>
#include "AArch64PltResolver.h"
#include "JmpCodeArch.h"

MOCKCPP_NS_START

#if defined(__aarch64__) && defined(__linux__)

namespace
{

void* mapAArch64TrampolinePage(const void* from, size_t pageSize)
{
   const uintptr_t fromAddress = reinterpret_cast<uintptr_t>(from);
   const uintptr_t sourcePage = fromAddress - fromAddress % pageSize;
   const uintptr_t branchRange = static_cast<uintptr_t>(1) << 27;

   // A non-fixed mmap hint never replaces an existing mapping.  Probe the
   // branch window at 64 KiB intervals so the kernel can choose a nearby gap
   // without relying on MAP_FIXED_NOREPLACE availability.
   uintptr_t step = static_cast<uintptr_t>(64) << 10;
   if(step < pageSize)
   {
      step = pageSize;
   }
   step -= step % pageSize;

   const size_t attempts = static_cast<size_t>(branchRange / step);
   for(size_t index = 0; index <= attempts; ++index)
   {
      const uintptr_t distance = static_cast<uintptr_t>(index) * step;
      for(unsigned int direction = 0; direction < 2; ++direction)
      {
         if(index == 0 && direction != 0)
         {
            continue;
         }

         uintptr_t hintAddress = 0;
         if(direction == 0)
         {
            if(sourcePage > UINTPTR_MAX - distance)
            {
               continue;
            }
            hintAddress = sourcePage + distance;
         }
         else
         {
            if(sourcePage < distance)
            {
               continue;
            }
            hintAddress = sourcePage - distance;
         }

         void* page = ::mmap(reinterpret_cast<void*>(hintAddress),
                             pageSize,
                             PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS,
                             -1,
                             0);
         if(page == MAP_FAILED)
         {
            continue;
         }
         if(page == 0)
         {
            ::munmap(page, pageSize);
            continue;
         }

         unsigned char branch[MAX_JMP_CODE_SIZE];
         if(buildAArch64JmpCode(branch, from, page) == sizeof(uint32_t))
         {
            return page;
         }

         ::munmap(page, pageSize);
      }
   }

   return 0;
}

void* createAArch64NearTrampoline(const void* from,
                                  const void* to,
                                  size_t* mappingSize)
{
   const long configuredPageSize = ::sysconf(_SC_PAGESIZE);
   if(configuredPageSize <= 0)
   {
      return 0;
   }

   const size_t pageSize = static_cast<size_t>(configuredPageSize);
   if(pageSize < MAX_JMP_CODE_SIZE)
   {
      return 0;
   }

   void* page = mapAArch64TrampolinePage(from, pageSize);
   if(page == 0)
   {
      return 0;
   }

   unsigned char trampoline[MAX_JMP_CODE_SIZE];
   const size_t trampolineSize = buildAArch64JmpCode(trampoline, page, to);
   ::memcpy(page, trampoline, trampolineSize);

#if defined(__GNUC__) || defined(__clang__)
   __builtin___clear_cache(static_cast<char*>(page),
                           static_cast<char*>(page) + trampolineSize);
#endif

   if(::mprotect(page, pageSize, PROT_READ | PROT_EXEC) != 0)
   {
      ::munmap(page, pageSize);
      return 0;
   }

   *mappingSize = pageSize;
   return page;
}

void releaseAArch64NearTrampoline(void* address, size_t mappingSize)
{
   if(address != 0)
   {
      ::munmap(address, mappingSize);
   }
}

}

#endif

struct JmpCodeImpl
{
   ////////////////////////////////////////////////
   JmpCodeImpl(const void* from, const void* to)
      : m_codeSize(0)
      , m_patchAddress(GET_JMP_CODE_PATCH_ADDRESS(from))
      , m_patchModuleHandle(0)
      , m_trampolineAddress(0)
      , m_trampolineSize(0)
      , m_hookInstalled(false)
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
#if defined(__linux__)
         if(resolved.availablePatchSize >= sizeof(uint32_t) &&
            isAArch64PatchRangeSafe(m_patchAddress, sizeof(uint32_t)))
         {
            m_trampolineAddress = createAArch64NearTrampoline(
               m_patchAddress, to, &m_trampolineSize);
            if(m_trampolineAddress != 0)
            {
               m_codeSize = buildAArch64JmpCode(
                  m_code, m_patchAddress, m_trampolineAddress);
            }
         }
#endif

         if(m_trampolineAddress != 0 &&
            m_codeSize == sizeof(uint32_t) &&
            m_codeSize <= resolved.availablePatchSize &&
            isAArch64PatchRangeSafe(m_patchAddress, m_codeSize))
         {
            // Keep the pinned module and trampoline until the hook has first
            // been restored.  Destruction may otherwise unmap executable code
            // while the target still branches to it.
         }
         else
         {
#if defined(__aarch64__) && defined(__linux__)
            releaseAArch64NearTrampoline(
               m_trampolineAddress, m_trampolineSize);
            m_trampolineAddress = 0;
            m_trampolineSize = 0;
#endif
            releaseAArch64PltModule(m_patchModuleHandle);
            m_patchModuleHandle = 0;
            m_patchAddress = const_cast<void*>(resolved.fallbackAddress);
            m_codeSize = buildAArch64JmpCode(m_code, m_patchAddress, to);
         }
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
#if defined(__aarch64__) && defined(__linux__)
      if(!m_hookInstalled)
      {
         releaseAArch64NearTrampoline(
            m_trampolineAddress, m_trampolineSize);
      }
#endif
      if(!m_hookInstalled)
      {
         releaseAArch64PltModule(m_patchModuleHandle);
      }
   }

   void markInstalled()
   {
      m_hookInstalled = true;
   }

   void markRestored()
   {
      if(!m_hookInstalled)
      {
         return;
      }

      m_hookInstalled = false;
      // A caller may already have fetched the old branch when reset restores
      // the target.  Published trampoline code must therefore stay mapped for
      // the process lifetime; reclaiming it requires a real quiescence/epoch
      // protocol which mockcpp does not currently have.
      m_trampolineAddress = 0;
      m_trampolineSize = 0;
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
   void* m_trampolineAddress;
   size_t m_trampolineSize;
   bool m_hookInstalled;
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

void
JmpCode::markInstalled()
{
   This->markInstalled();
}

void
JmpCode::markRestored()
{
   This->markRestored();
}

MOCKCPP_NS_END
