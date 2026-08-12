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
#include <string.h>

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

bool rangeWithinSegment(uintptr_t segmentStart,
                        uintptr_t segmentSize,
                        uintptr_t address,
                        size_t size)
{
   if(address < segmentStart ||
      size > segmentSize ||
      address - segmentStart > segmentSize - size)
   {
      return false;
   }

   return true;
}

struct LoadedImageView
{
   LoadedImageView()
      : loadBias(0)
      , phdrs(0)
      , phnum(0)
      , dynamic(0)
      , dynamicCount(0)
   {
   }

   uintptr_t loadBias;
   const ElfW(Phdr)* phdrs;
   size_t phnum;
   const ElfW(Dyn)* dynamic;
   size_t dynamicCount;
};

bool imageContainsRange(const LoadedImageView& image,
                        const void* address,
                        size_t size,
                        ElfW(Word) requiredFlags)
{
   if(address == 0 || size == 0)
   {
      return false;
   }

   const uintptr_t rangeAddress = reinterpret_cast<uintptr_t>(address);
   for(size_t i = 0; i < image.phnum; ++i)
   {
      const ElfW(Phdr)& header = image.phdrs[i];
      if(header.p_type != PT_LOAD ||
         (header.p_flags & requiredFlags) != requiredFlags)
      {
         continue;
      }

      const uintptr_t offset = static_cast<uintptr_t>(header.p_vaddr);
      if(image.loadBias > UINTPTR_MAX - offset)
      {
         continue;
      }

      const uintptr_t start = image.loadBias + offset;
      const uintptr_t segmentSize = static_cast<uintptr_t>(header.p_memsz);
      if(rangeWithinSegment(start, segmentSize, rangeAddress, size))
      {
         return true;
      }
   }

   return false;
}

struct LoadedImageQuery
{
   LoadedImageQuery(const struct link_map* map, const void* value)
      : image(map)
      , address(reinterpret_cast<uintptr_t>(value))
      , found(false)
   {
   }

   const struct link_map* image;
   uintptr_t address;
   LoadedImageView view;
   bool found;
};

int findLoadedImage(struct dl_phdr_info* info, size_t, void* data)
{
   LoadedImageQuery* query = static_cast<LoadedImageQuery*>(data);
   if(static_cast<ElfW(Addr)>(info->dlpi_addr) != query->image->l_addr)
   {
      return 0;
   }

   bool containsAddress = false;
   const ElfW(Phdr)* dynamicHeader = 0;
   for(ElfW(Half) i = 0; i < info->dlpi_phnum; ++i)
   {
      const ElfW(Phdr)& header = info->dlpi_phdr[i];
      const uintptr_t offset = static_cast<uintptr_t>(header.p_vaddr);
      if(static_cast<uintptr_t>(info->dlpi_addr) > UINTPTR_MAX - offset)
      {
         continue;
      }

      const uintptr_t start =
         static_cast<uintptr_t>(info->dlpi_addr) + offset;
      if(header.p_type == PT_LOAD &&
         rangeWithinSegment(start,
                            static_cast<uintptr_t>(header.p_memsz),
                            query->address,
                            1))
      {
         containsAddress = true;
      }
      else if(header.p_type == PT_DYNAMIC)
      {
         if(dynamicHeader != 0)
         {
            return 0;
         }
         dynamicHeader = &header;
      }
   }

   if(!containsAddress || dynamicHeader == 0 ||
      dynamicHeader->p_filesz < sizeof(ElfW(Dyn)) ||
      dynamicHeader->p_filesz > dynamicHeader->p_memsz ||
      dynamicHeader->p_filesz % sizeof(ElfW(Dyn)) != 0)
   {
      return 0;
   }

   const uintptr_t dynamicOffset =
      static_cast<uintptr_t>(dynamicHeader->p_vaddr);
   const uintptr_t loadBias = static_cast<uintptr_t>(info->dlpi_addr);
   if(loadBias > UINTPTR_MAX - dynamicOffset)
   {
      return 0;
   }

   query->view.loadBias = loadBias;
   query->view.phdrs = info->dlpi_phdr;
   query->view.phnum = info->dlpi_phnum;
   query->view.dynamic = reinterpret_cast<const ElfW(Dyn)*>(
      loadBias + dynamicOffset);
   query->view.dynamicCount = static_cast<size_t>(
      dynamicHeader->p_filesz / sizeof(ElfW(Dyn)));

   if(query->view.dynamic != query->image->l_ld ||
      !imageContainsRange(query->view,
                          query->view.dynamic,
                          static_cast<size_t>(dynamicHeader->p_filesz),
                          PF_R))
   {
      return 0;
   }

   query->found = true;
   return 1;
}

bool getLoadedImageView(const void* address,
                        const struct link_map* image,
                        LoadedImageView* view)
{
   LoadedImageQuery query(image, address);
   ::dl_iterate_phdr(findLoadedImage, &query);
   if(!query.found)
   {
      return false;
   }

   *view = query.view;
   return true;
}

bool isMainExecutableImage(const Dl_info& info,
                           const struct link_map* image,
                           const LoadedImageView& view)
{
   if(info.dli_fbase == 0 || image == 0 || image->l_addr != 0)
   {
      return false;
   }

   if(!imageContainsRange(view, info.dli_fbase, sizeof(Elf64_Ehdr), PF_R))
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

struct DynamicTables
{
   DynamicTables()
      : symbolTable(0)
      , symbolEntrySize(0)
      , stringTable(0)
      , stringTableSize(0)
      , versionTable(0)
      , versionNeeds(0)
      , versionNeedCount(0)
   {
   }

   const ElfW(Sym)* symbolTable;
   size_t symbolEntrySize;
   const char* stringTable;
   size_t stringTableSize;
   const ElfW(Half)* versionTable;
   const ElfW(Verneed)* versionNeeds;
   size_t versionNeedCount;
};

template <typename T>
bool setUniqueValue(T* destination, bool* found, const T& value)
{
   if(*found && *destination != value)
   {
      return false;
   }

   *destination = value;
   *found = true;
   return true;
}

bool readDynamicTables(const LoadedImageView& image, DynamicTables* tables)
{
   // This resolver only accepts a non-PIE ET_EXEC image.  Its address-valued
   // dynamic entries are therefore already absolute runtime addresses.
   if(image.loadBias != 0)
   {
      return false;
   }

   bool foundTerminator = false;
   bool foundSymbolTable = false;
   bool foundSymbolEntrySize = false;
   bool foundStringTable = false;
   bool foundStringTableSize = false;
   bool foundVersionTable = false;
   bool foundVersionNeeds = false;
   bool foundVersionNeedCount = false;

   for(size_t i = 0; i < image.dynamicCount; ++i)
   {
      const ElfW(Dyn)* entry = image.dynamic + i;
      if(entry->d_tag == DT_NULL)
      {
         foundTerminator = true;
         break;
      }

      if(entry->d_tag == DT_SYMTAB)
      {
         const ElfW(Sym)* value = reinterpret_cast<const ElfW(Sym)*>(
            static_cast<uintptr_t>(entry->d_un.d_ptr));
         if(!setUniqueValue(&tables->symbolTable,
                            &foundSymbolTable,
                            value))
         {
            return false;
         }
      }
      else if(entry->d_tag == DT_SYMENT)
      {
         const size_t value = static_cast<size_t>(entry->d_un.d_val);
         if(!setUniqueValue(&tables->symbolEntrySize,
                            &foundSymbolEntrySize,
                            value))
         {
            return false;
         }
      }
      else if(entry->d_tag == DT_STRTAB)
      {
         const char* value = reinterpret_cast<const char*>(
            static_cast<uintptr_t>(entry->d_un.d_ptr));
         if(!setUniqueValue(&tables->stringTable,
                            &foundStringTable,
                            value))
         {
            return false;
         }
      }
      else if(entry->d_tag == DT_STRSZ)
      {
         const size_t value = static_cast<size_t>(entry->d_un.d_val);
         if(!setUniqueValue(&tables->stringTableSize,
                            &foundStringTableSize,
                            value))
         {
            return false;
         }
      }
#ifdef DT_VERSYM
      else if(entry->d_tag == DT_VERSYM)
      {
         const ElfW(Half)* value = reinterpret_cast<const ElfW(Half)*>(
            static_cast<uintptr_t>(entry->d_un.d_ptr));
         if(!setUniqueValue(&tables->versionTable,
                            &foundVersionTable,
                            value))
         {
            return false;
         }
      }
#endif
      else if(entry->d_tag == DT_VERNEED)
      {
         const ElfW(Verneed)* value =
            reinterpret_cast<const ElfW(Verneed)*>(
               static_cast<uintptr_t>(entry->d_un.d_ptr));
         if(!setUniqueValue(&tables->versionNeeds,
                            &foundVersionNeeds,
                            value))
         {
            return false;
         }
      }
      else if(entry->d_tag == DT_VERNEEDNUM)
      {
         const size_t value = static_cast<size_t>(entry->d_un.d_val);
         if(!setUniqueValue(&tables->versionNeedCount,
                            &foundVersionNeedCount,
                            value))
         {
            return false;
         }
      }
   }

   if(!foundTerminator ||
      !foundSymbolTable || tables->symbolTable == 0 ||
      !foundSymbolEntrySize ||
      tables->symbolEntrySize != sizeof(ElfW(Sym)) ||
      !foundStringTable || tables->stringTable == 0 ||
      !foundStringTableSize || tables->stringTableSize == 0 ||
      (foundVersionTable && tables->versionTable == 0) ||
      foundVersionNeeds != foundVersionNeedCount ||
      (foundVersionNeeds &&
       (tables->versionNeeds == 0 || tables->versionNeedCount == 0)) ||
      !imageContainsRange(image,
                          tables->symbolTable,
                          sizeof(*tables->symbolTable),
                          PF_R) ||
      !imageContainsRange(image,
                          tables->stringTable,
                          tables->stringTableSize,
                          PF_R))
   {
      return false;
   }

   return true;
}

const char* boundedString(const DynamicTables& tables, size_t offset)
{
   if(offset >= tables.stringTableSize)
   {
      return 0;
   }

   const char* value = tables.stringTable + offset;
   const size_t remaining = tables.stringTableSize - offset;
   return ::memchr(value, '\0', remaining) == 0 ? 0 : value;
}

template <typename T>
const T* checkedArrayElement(const T* array, size_t index)
{
   const uintptr_t start = reinterpret_cast<uintptr_t>(array);
   if(index > (UINTPTR_MAX - start) / sizeof(T))
   {
      return 0;
   }

   return reinterpret_cast<const T*>(start + index * sizeof(T));
}

template <typename T, typename Base>
const T* checkedRelativePointer(const Base* base, size_t offset)
{
   const uintptr_t start = reinterpret_cast<uintptr_t>(base);
   if(start > UINTPTR_MAX - offset)
   {
      return 0;
   }

   return reinterpret_cast<const T*>(start + offset);
}

struct ImportedSymbolLookup
{
   ImportedSymbolLookup()
      : name(0), version(0)
   {
   }

   const char* name;
   const char* version;
};

bool getImportedSymbolLookup(const ElfW(Sym)* symbol,
                             const LoadedImageView& image,
                             ImportedSymbolLookup* lookup)
{
   DynamicTables tables;
   if(!readDynamicTables(image, &tables) ||
      !imageContainsRange(image, symbol, sizeof(*symbol), PF_R))
   {
      return false;
   }

   const uintptr_t symbolAddress = reinterpret_cast<uintptr_t>(symbol);
   const uintptr_t tableAddress =
      reinterpret_cast<uintptr_t>(tables.symbolTable);
   if(symbolAddress < tableAddress)
   {
      return false;
   }

   const uintptr_t byteOffset = symbolAddress - tableAddress;
   if(byteOffset % tables.symbolEntrySize != 0)
   {
      return false;
   }

   lookup->name = boundedString(tables, symbol->st_name);
   if(lookup->name == 0 || lookup->name[0] == '\0')
   {
      return false;
   }

   if(tables.versionTable == 0)
   {
      return true;
   }

   const size_t symbolIndex = byteOffset / tables.symbolEntrySize;
   const ElfW(Half)* versionEntry =
      checkedArrayElement(tables.versionTable, symbolIndex);
   if(versionEntry == 0 ||
      !imageContainsRange(image, versionEntry, sizeof(*versionEntry), PF_R))
   {
      return false;
   }

   const unsigned int versionIndex = *versionEntry & 0x7fffU;
   if(versionIndex == VER_NDX_LOCAL || versionIndex == VER_NDX_GLOBAL)
   {
      return true;
   }

   if(tables.versionNeeds == 0 || tables.versionNeedCount == 0 ||
      tables.versionNeedCount > 0x7fffU)
   {
      return false;
   }

   const ElfW(Verneed)* need = tables.versionNeeds;
   const char* matchedVersion = 0;
   size_t totalAuxiliaryCount = 0;
   for(size_t needIndex = 0;
       needIndex < tables.versionNeedCount;
       ++needIndex)
   {
      if(!imageContainsRange(image, need, sizeof(*need), PF_R) ||
         need->vn_version != VER_NEED_CURRENT || need->vn_cnt == 0 ||
         need->vn_cnt > 0x7fffU ||
         totalAuxiliaryCount > 0x7fffU - need->vn_cnt ||
         need->vn_aux < sizeof(*need) ||
         need->vn_aux % __alignof__(ElfW(Vernaux)) != 0)
      {
         return false;
      }

      const char* neededFile = boundedString(tables, need->vn_file);
      if(neededFile == 0 || neededFile[0] == '\0')
      {
         return false;
      }
      totalAuxiliaryCount += need->vn_cnt;

      const ElfW(Vernaux)* auxiliary =
         checkedRelativePointer<ElfW(Vernaux)>(need, need->vn_aux);
      if(auxiliary == 0)
      {
         return false;
      }

      for(ElfW(Half) auxiliaryIndex = 0;
          auxiliaryIndex < need->vn_cnt;
          ++auxiliaryIndex)
      {
         if(!imageContainsRange(image,
                                auxiliary,
                                sizeof(*auxiliary),
                                PF_R))
         {
            return false;
         }

         const char* versionName =
            boundedString(tables, auxiliary->vna_name);
         if(versionName == 0 || versionName[0] == '\0')
         {
            return false;
         }

         if((auxiliary->vna_other & 0x7fffU) == versionIndex)
         {
            if(matchedVersion != 0)
            {
               return false;
            }
            matchedVersion = versionName;
         }

         if(auxiliaryIndex + 1 < need->vn_cnt)
         {
            if(auxiliary->vna_next < sizeof(*auxiliary) ||
               auxiliary->vna_next % __alignof__(ElfW(Vernaux)) != 0)
            {
               return false;
            }
            auxiliary = checkedRelativePointer<ElfW(Vernaux)>(
               auxiliary, auxiliary->vna_next);
            if(auxiliary == 0)
            {
               return false;
            }
         }
         else if(auxiliary->vna_next != 0)
         {
            return false;
         }
      }

      if(needIndex + 1 < tables.versionNeedCount)
      {
         if(need->vn_next < sizeof(*need) ||
            need->vn_next % __alignof__(ElfW(Verneed)) != 0)
         {
            return false;
         }
         need = checkedRelativePointer<ElfW(Verneed)>(need, need->vn_next);
         if(need == 0)
         {
            return false;
         }
      }
      else if(need->vn_next != 0)
      {
         return false;
      }
   }

   lookup->version = matchedVersion;
   return lookup->version != 0;
}

void* lookupRuntimeSymbol(void* handle, const ImportedSymbolLookup& lookup)
{
   return lookup.version == 0
      ? ::dlsym(handle, lookup.name)
      : ::dlvsym(handle, lookup.name, lookup.version);
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
   LoadedImageView sourceImageView;
   ImportedSymbolLookup lookup;
   if(::dladdr1(address, &linkInfo, &linkData, RTLD_DL_LINKMAP) == 0 ||
      linkData == 0 || linkInfo.dli_fbase != sourceInfo.dli_fbase)
   {
      return sourceFallback;
   }

   const struct link_map* sourceImage =
      static_cast<const struct link_map*>(linkData);
   if(!getLoadedImageView(address, sourceImage, &sourceImageView) ||
      !isMainExecutableImage(sourceInfo, sourceImage, sourceImageView) ||
      !getImportedSymbolLookup(symbol, sourceImageView, &lookup) ||
      ::strcmp(lookup.name, sourceInfo.dli_sname) != 0)
   {
      return sourceFallback;
   }

   ::dlerror();
   void* resolved = lookupRuntimeSymbol(RTLD_NEXT, lookup);
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
   void* pinnedAddress = lookupRuntimeSymbol(moduleHandle, lookup);
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
