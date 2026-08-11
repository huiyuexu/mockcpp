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
#ifndef __MOCKCPP_JMP_CODE_AARCH64_H__
#define __MOCKCPP_JMP_CODE_AARCH64_H__

// Far jump: ldr x16, #8; br x16; .quad target
// x16 is an intra-procedure-call scratch register under AAPCS64.
const unsigned char aarch64AbsoluteJmpCodeTemplate[] =
   { 0x50, 0x00, 0x00, 0x58, 0x00, 0x02, 0x1F, 0xD6,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

const size_t MAX_JMP_CODE_SIZE = sizeof(aarch64AbsoluteJmpCodeTemplate);

inline size_t buildAArch64JmpCode(unsigned char* code,
                                  const void* from,
                                  const void* to)
{
   const uintptr_t fromAddress = reinterpret_cast<uintptr_t>(from);
   const uintptr_t targetAddress = reinterpret_cast<uintptr_t>(to);

   // B uses a signed imm26 scaled by four: [-128 MiB, 128 MiB).
   // Prefer it because a valid AArch64 function may be only 4 or 8 bytes.
   uintptr_t distance = 0;
   uint32_t immediate = 0;
   bool canUseDirectBranch = false;

   if(targetAddress >= fromAddress)
   {
      distance = targetAddress - fromAddress;
      canUseDirectBranch = (distance & 3U) == 0 &&
                           distance < (static_cast<uintptr_t>(1) << 27);
      immediate = static_cast<uint32_t>(distance >> 2);
   }
   else
   {
      distance = fromAddress - targetAddress;
      canUseDirectBranch = (distance & 3U) == 0 &&
                           distance <= (static_cast<uintptr_t>(1) << 27);
      immediate = 0U - static_cast<uint32_t>(distance >> 2);
   }

   if(canUseDirectBranch)
   {
      const uint32_t branch = 0x14000000U | (immediate & 0x03FFFFFFU);
      // A64 instructions are always fetched little-endian, including on
      // big-endian AArch64 systems.
      code[0] = static_cast<unsigned char>(branch);
      code[1] = static_cast<unsigned char>(branch >> 8);
      code[2] = static_cast<unsigned char>(branch >> 16);
      code[3] = static_cast<unsigned char>(branch >> 24);
      return sizeof(branch);
   }

   ::memcpy(code, aarch64AbsoluteJmpCodeTemplate,
            sizeof(aarch64AbsoluteJmpCodeTemplate));
   ::memcpy(code + 8, &targetAddress, sizeof(targetAddress));
   return sizeof(aarch64AbsoluteJmpCodeTemplate);
}

#define GET_JMP_CODE_PATCH_ADDRESS(from) (const_cast<void*>(from))

#endif
