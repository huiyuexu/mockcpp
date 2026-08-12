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

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#if defined(__aarch64__) && defined(__linux__)
#  include <features.h>
#endif

#include "AArch64PltResolver.h"

#if defined(__aarch64__) && defined(__linux__) && defined(__GLIBC__)

#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <stddef.h>
#include <stdint.h>

#endif

MOCKCPP_NS_START

#if defined(__aarch64__) && defined(__linux__) && defined(__GLIBC__)

namespace
{

struct SegmentQuery
{
   SegmentQuery(const void* value, size_t rangeSize)
      : address(reinterpret_cast<uintptr_t>(value))
      , size(static_cast<uintptr_t>(rangeSize))
      , found(false)
   {
   }

   uintptr_t address;
   uintptr_t size;
   bool found;
};

int findReadableExecutableSegment(struct dl_phdr_info* info,
                                  size_t,
                                  void* data)
{
   SegmentQuery* query = static_cast<SegmentQuery*>(data);

   for(ElfW(Half) i = 0; i < info->dlpi_phnum; ++i)
   {
      const ElfW(Phdr)& header = info->dlpi_phdr[i];
      if(header.p_type != PT_LOAD ||
         (header.p_flags & (PF_R | PF_X)) != (PF_R | PF_X))
      {
         continue;
      }

      const uintptr_t base = static_cast<uintptr_t>(info->dlpi_addr);
      const uintptr_t offset = static_cast<uintptr_t>(header.p_vaddr);
      if(base > UINTPTR_MAX - offset)
      {
         continue;
      }

      const uintptr_t start = base + offset;
      const uintptr_t size = static_cast<uintptr_t>(header.p_memsz);
      if(size < 4 || start > UINTPTR_MAX - size)
      {
         continue;
      }

      if(query->size == 0 ||
         query->address > UINTPTR_MAX - query->size)
      {
         continue;
      }

      const uintptr_t end = start + size;
      const uintptr_t queryEnd = query->address + query->size;
      if(query->address >= start && queryEnd <= end)
      {
         query->found = true;
         return 1;
      }
   }

   return 0;
}

bool isReadableExecutableRange(const void* address, size_t size)
{
   SegmentQuery query(address, size);
   ::dl_iterate_phdr(findReadableExecutableSegment, &query);
   return query.found;
}

const void* dynamicAddress(const struct link_map* image, ElfW(Addr) value)
{
   // Canonical PLT resolution is deliberately restricted to an ET_EXEC
   // image with l_addr == 0, so its dynamic pointers are already absolute.
   if(image->l_addr != 0)
   {
      return 0;
   }

   return reinterpret_cast<const void*>(static_cast<uintptr_t>(value));
}

bool isMainExecutableImage(const Dl_info& info,
                           const struct link_map* image)
{
   if(info.dli_fbase == 0 || image == 0 || image->l_addr != 0)
   {
      return false;
   }

   const Elf64_Ehdr* header =
      static_cast<const Elf64_Ehdr*>(info.dli_fbase);
   return header->e_ident[EI_MAG0] == ELFMAG0 &&
          header->e_ident[EI_MAG1] == ELFMAG1 &&
          header->e_ident[EI_MAG2] == ELFMAG2 &&
          header->e_ident[EI_MAG3] == ELFMAG3 &&
          header->e_ident[EI_CLASS] == ELFCLASS64 &&
          header->e_ident[EI_DATA] == ELFDATA2LSB &&
          header->e_type == ET_EXEC &&
          header->e_machine == EM_AARCH64;
}

bool isUnversionedSymbol(const ElfW(Sym)* symbol,
                         const struct link_map* image)
{
   if(image == 0 || image->l_ld == 0)
   {
      return false;
   }

   const ElfW(Sym)* symbolTable = 0;
   const ElfW(Half)* versionTable = 0;

   for(const ElfW(Dyn)* entry = image->l_ld;
       entry->d_tag != DT_NULL;
       ++entry)
   {
      if(entry->d_tag == DT_SYMTAB)
      {
         symbolTable = static_cast<const ElfW(Sym)*>(
            dynamicAddress(image, entry->d_un.d_ptr));
      }
#ifdef DT_VERSYM
      else if(entry->d_tag == DT_VERSYM)
      {
         versionTable = static_cast<const ElfW(Half)*>(
            dynamicAddress(image, entry->d_un.d_ptr));
      }
#endif
   }

   if(symbolTable == 0)
   {
      return false;
   }

   const uintptr_t symbolAddress = reinterpret_cast<uintptr_t>(symbol);
   const uintptr_t tableAddress = reinterpret_cast<uintptr_t>(symbolTable);
   if(symbolAddress < tableAddress)
   {
      return false;
   }

   const uintptr_t byteOffset = symbolAddress - tableAddress;
   if(byteOffset % sizeof(ElfW(Sym)) != 0)
   {
      return false;
   }

   if(versionTable == 0)
   {
      return true;
   }

   const size_t symbolIndex = byteOffset / sizeof(ElfW(Sym));
   const ElfW(Half) version = versionTable[symbolIndex] & 0x7fffU;
   return version <= 1;
}

bool beginsWithBti(const void* address)
{
   const unsigned char* instruction =
      static_cast<const unsigned char*>(address);

   // All four BTI variants have the little-endian byte pattern
   // { 0x1f/0x5f/0x9f/0xdf, 0x24, 0x03, 0xd5 }.
   return (instruction[0] & 0x3fU) == 0x1fU &&
          instruction[1] == 0x24U &&
          instruction[2] == 0x03U &&
          instruction[3] == 0xd5U;
}

bool beginsWithPacReturnLandingPad(const void* address)
{
   const unsigned char* instruction =
      static_cast<const unsigned char*>(address);

   // PACIASP and PACIBSP can serve as call landing pads on a BTI guarded
   // page.  They cannot be preserved before a jump to a normal C++ stub,
   // because that would pass a signed LR to code which does not authenticate.
   return (instruction[0] == 0x3fU || instruction[0] == 0x7fU) &&
          instruction[1] == 0x23U &&
          instruction[2] == 0x03U &&
          instruction[3] == 0xd5U;
}

AArch64PltResolution canonicalPltFallback(const void* address)
{
   const unsigned char* patchAddress =
      static_cast<const unsigned char*>(address);
   if(isReadableExecutableRange(address, 4) && beginsWithBti(address))
   {
      // A function pointer can branch indirectly to a canonical PLT entry.
      // Keep its BTI landing pad just as we do for the resolved SO target.
      patchAddress += 4;
   }

   return AArch64PltResolution(patchAddress, 0);
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
AArch64PltResolution resolveCanonicalPltAddress(const void* address)
{
   Dl_info sourceInfo;
   void* symbolData = 0;
   if(::dladdr1(address, &sourceInfo, &symbolData, RTLD_DL_SYMENT) == 0 ||
      symbolData == 0 || sourceInfo.dli_saddr != address ||
      sourceInfo.dli_sname == 0 || sourceInfo.dli_sname[0] == '\0')
   {
      return AArch64PltResolution(address, 0);
   }

   const ElfW(Sym)* symbol = static_cast<const ElfW(Sym)*>(symbolData);
   const unsigned int symbolType = ELF64_ST_TYPE(symbol->st_info);
   const unsigned int symbolBinding = ELF64_ST_BIND(symbol->st_info);
   if(symbolType != STT_FUNC ||
      (symbolBinding != STB_GLOBAL && symbolBinding != STB_WEAK) ||
      symbol->st_shndx != SHN_UNDEF || symbol->st_value == 0)
   {
      return AArch64PltResolution(address, 0);
   }

   const AArch64PltResolution sourceFallback =
      canonicalPltFallback(address);

   Dl_info resolverInfo;
   if(::dladdr(reinterpret_cast<const void*>(&resolveCanonicalPltAddress),
               &resolverInfo) == 0 ||
      sourceInfo.dli_fbase != resolverInfo.dli_fbase)
   {
      // RTLD_NEXT starts after the ELF object containing the caller.  The
      // canonical PLT and this resolver must therefore be in the same image.
      return sourceFallback;
   }

   Dl_info linkInfo;
   void* linkData = 0;
   if(::dladdr1(address, &linkInfo, &linkData, RTLD_DL_LINKMAP) == 0 ||
      linkData == 0 || linkInfo.dli_fbase != sourceInfo.dli_fbase ||
      !isMainExecutableImage(
         sourceInfo, static_cast<const struct link_map*>(linkData)) ||
      !isUnversionedSymbol(symbol,
                           static_cast<const struct link_map*>(linkData)))
   {
      // dlsym() cannot express the exact relocation version without parsing
      // the caller's version definition.  Do not guess for versioned imports.
      return sourceFallback;
   }

   ::dlerror();
   void* resolved = ::dlsym(RTLD_NEXT, sourceInfo.dli_sname);
   const char* resolveError = ::dlerror();
   if(resolveError != 0 || resolved == 0 || resolved == address)
   {
      return sourceFallback;
   }

   Dl_info targetInfo;
   if(::dladdr(resolved, &targetInfo) == 0 ||
      targetInfo.dli_fbase == sourceInfo.dli_fbase ||
      targetInfo.dli_fname == 0 || targetInfo.dli_fname[0] == '\0' ||
      !isReadableExecutableRange(resolved, 4))
   {
      return sourceFallback;
   }

   Dl_info targetSymbolInfo;
   void* targetSymbolData = 0;
   if(::dladdr1(resolved, &targetSymbolInfo, &targetSymbolData,
                RTLD_DL_SYMENT) == 0 ||
      targetSymbolData == 0 || targetSymbolInfo.dli_saddr != resolved ||
      targetSymbolInfo.dli_fbase != targetInfo.dli_fbase)
   {
      return sourceFallback;
   }

   const ElfW(Sym)* targetSymbol =
      static_cast<const ElfW(Sym)*>(targetSymbolData);
   if(ELF64_ST_TYPE(targetSymbol->st_info) != STT_FUNC ||
      targetSymbol->st_shndx == SHN_UNDEF || targetSymbol->st_size == 0 ||
      beginsWithPacReturnLandingPad(resolved))
   {
      return sourceFallback;
   }

   void* moduleHandle =
      ::dlopen(targetInfo.dli_fname, RTLD_NOLOAD | RTLD_LAZY);
   if(moduleHandle == 0)
   {
      return sourceFallback;
   }

   ::dlerror();
   void* pinnedAddress = ::dlsym(moduleHandle, sourceInfo.dli_sname);
   const char* pinError = ::dlerror();
   Dl_info pinnedInfo;
   if(pinError != 0 || pinnedAddress != resolved ||
      ::dladdr(pinnedAddress, &pinnedInfo) == 0 ||
      pinnedInfo.dli_fbase != targetInfo.dli_fbase)
   {
      ::dlclose(moduleHandle);
      return sourceFallback;
   }

   const unsigned char* patchAddress =
      static_cast<const unsigned char*>(resolved);
   size_t availablePatchSize = static_cast<size_t>(targetSymbol->st_size);
   if(beginsWithBti(resolved))
   {
      // Preserve the indirect-call landing pad and install the jump after it.
      patchAddress += 4;
      if(availablePatchSize <= 4)
      {
         ::dlclose(moduleHandle);
         return sourceFallback;
      }
      availablePatchSize -= 4;
   }

   return AArch64PltResolution(patchAddress,
                               moduleHandle,
                               availablePatchSize,
                               sourceFallback.address);
}

}

#endif

AArch64PltResolution resolveAArch64PltAddress(const void* address)
{
#if defined(__aarch64__) && defined(__linux__) && defined(__GLIBC__)
   return resolveCanonicalPltAddress(address);
#else
   return AArch64PltResolution(address, 0);
#endif
}

bool isAArch64PatchRangeSafe(const void* address, size_t size)
{
#if defined(__aarch64__) && defined(__linux__) && defined(__GLIBC__)
   return isReadableExecutableRange(address, size);
#else
   (void)address;
   (void)size;
   return true;
#endif
}

void releaseAArch64PltModule(void* moduleHandle)
{
#if defined(__aarch64__) && defined(__linux__) && defined(__GLIBC__)
   if(moduleHandle != 0)
   {
      ::dlclose(moduleHandle);
   }
#else
   (void)moduleHandle;
#endif
}

MOCKCPP_NS_END
