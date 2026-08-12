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

#include <testngpp/testngpp.hpp>
#include <stdint.h>
#include <stdlib.h>

#include <mockcpp/JmpCode.h>

USING_MOCKCPP_NS
USING_TESTNGPP_NS

#if defined(__linux__)

#include <sys/mman.h>
#include <unistd.h>

#include <mockcpp/CodeModifier.h>

FIXTURE(CodeModifierPageRange)
{
   TEST(does not require an untouched following page to be mapped)
   {
      const size_t pageSize = static_cast<size_t>(getpagesize());
      unsigned char* pages = static_cast<unsigned char*>(
         ::mmap(0, pageSize * 2, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

      ASSERT_TRUE(pages != static_cast<unsigned char*>(MAP_FAILED));
      pages[0] = 0;

      ASSERT_EQ(0, ::munmap(pages + pageSize, pageSize));
      ASSERT_EQ(0, ::mprotect(pages, pageSize, PROT_READ | PROT_EXEC));

      const unsigned char replacement = 0x5A;
      ASSERT_TRUE(CodeModifier::modify(
         pages, &replacement, sizeof(replacement)));
      ASSERT_EQ(replacement, pages[0]);

      ASSERT_EQ(0, ::munmap(pages, pageSize));
   }
};

#endif

#if defined(__aarch64__) || defined(_M_ARM64)

FIXTURE(AArch64JmpCode)
{
   TEST(uses one branch instruction for a nearby target)
   {
      const void* from = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x100000));
      const void* to = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x100100));
      JmpCode jmpCode(from, to);

      ASSERT_EQ(sizeof(uint32_t), jmpCode.getCodeSize());

      const unsigned char* branch = static_cast<const unsigned char*>(
         jmpCode.getCodeData());
      ASSERT_EQ(static_cast<unsigned char>(0x40), branch[0]);
      ASSERT_EQ(static_cast<unsigned char>(0x00), branch[1]);
      ASSERT_EQ(static_cast<unsigned char>(0x00), branch[2]);
      ASSERT_EQ(static_cast<unsigned char>(0x14), branch[3]);
   }

   TEST(encodes a nearby backward branch)
   {
      const void* from = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x100100));
      const void* to = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x100000));
      JmpCode jmpCode(from, to);

      ASSERT_EQ(sizeof(uint32_t), jmpCode.getCodeSize());

      const unsigned char* branch = static_cast<const unsigned char*>(
         jmpCode.getCodeData());
      ASSERT_EQ(static_cast<unsigned char>(0xC0), branch[0]);
      ASSERT_EQ(static_cast<unsigned char>(0xFF), branch[1]);
      ASSERT_EQ(static_cast<unsigned char>(0xFF), branch[2]);
      ASSERT_EQ(static_cast<unsigned char>(0x17), branch[3]);
   }

   TEST(handles the direct branch range boundaries)
   {
      const void* from = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x10000000));
      const void* highest = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x17FFFFFC));
      const void* lowest = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x08000000));
      const void* tooHigh = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x18000000));

      JmpCode highestBranch(from, highest);
      JmpCode lowestBranch(from, lowest);
      JmpCode absoluteBranch(from, tooHigh);

      ASSERT_EQ(sizeof(uint32_t), highestBranch.getCodeSize());
      ASSERT_EQ(sizeof(uint32_t), lowestBranch.getCodeSize());
      ASSERT_EQ(static_cast<size_t>(16), absoluteBranch.getCodeSize());
   }

   TEST(uses the absolute sequence when the target is unaligned)
   {
      const void* from = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x100000));
      const void* unaligned = reinterpret_cast<const void*>(
         static_cast<uintptr_t>(0x100002));
      JmpCode jmpCode(from, unaligned);

      ASSERT_EQ(static_cast<size_t>(16), jmpCode.getCodeSize());
   }
};

#if defined(MOCKCPP_AARCH64_CROSS_SO_TESTS)

FIXTURE(AArch64CrossSoHook)
{
   TEST(resolves a lazy canonical PLT entry to the function in its shared object)
   {
      ASSERT_EQ(0, ::system("./cross_so/mockcpp-aarch64-cross-so-lazy"));
   }

   TEST(resolves an eagerly bound canonical PLT entry to the function in its shared object)
   {
      ASSERT_EQ(0, ::system("./cross_so/mockcpp-aarch64-cross-so-now"));
   }
};

#endif

#endif
