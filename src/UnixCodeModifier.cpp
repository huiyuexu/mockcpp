/***
   mockcpp is a C/C++ mock framework.
   Copyright [2008] [Darwin Yuan <darwin.yuan@gmail.com>]
                    [Chen Guodong <sinojelly@gmail.com>]
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

#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>

#include <mockcpp/CodeModifier.h>

MOCKCPP_NS_START

bool CodeModifier::modify(void *dest, const void *src, size_t size)
{
    if(size == 0)
    {
       return true;
    }

    const size_t page_size = static_cast<size_t>(getpagesize());
    const uintptr_t page_mask = static_cast<uintptr_t>(page_size - 1);
    const uintptr_t first_page = reinterpret_cast<uintptr_t>(dest) & ~page_mask;
    const uintptr_t last_page =
       (reinterpret_cast<uintptr_t>(dest) + size - 1) & ~page_mask;
    // mprotect fails if any page in the requested range is unmapped.
    // Protect only the pages that the patch actually touches.
    const size_t protected_size =
       static_cast<size_t>(last_page - first_page) + page_size;

    if(::mprotect(reinterpret_cast<void*>(first_page), protected_size,
                  PROT_EXEC | PROT_WRITE | PROT_READ) != 0)
    {
       return false;
    }

    ::memcpy(dest, src, size);

#if defined(__GNUC__) || defined(__clang__)
    __builtin___clear_cache(static_cast<char*>(dest),
                            static_cast<char*>(dest) + size);
#endif


#if 0
	#if BUILD_FOR_X86
	//(void)memcpy(dest, src, size); // something wrong on linux: after memcpy(or 5 single byte copy),  the 4 bytes following jmp, src is 0x07c951b0, but dest is 0x07b851b0. so use unsigned int *, it works ok.
	*((unsigned char *)dest) = *((unsigned char *)src);
	*((unsigned long *)((unsigned long)dest + 1)) = *((unsigned long *)((unsigned long)src + 1));
	#else
	*((unsigned char *)dest) = *((unsigned char *)src);
	*((unsigned char *)((unsigned long)dest + 1)) = *((unsigned char *)((unsigned long)src + 1));
    // after this line, dest+2 is 0x00c90000, not 0, so change it.
	*((unsigned char *)((unsigned long)dest + 2)) = *((unsigned char *)((unsigned long)src + 2));
	*((unsigned char *)((unsigned long)dest + 3)) = *((unsigned char *)((unsigned long)src + 3));
	*((unsigned char *)((unsigned long)dest + 4)) = *((unsigned char *)((unsigned long)src + 4));
	*((unsigned char *)((unsigned long)dest + 5)) = *((unsigned char *)((unsigned long)src + 5));
	*((unsigned long *)((unsigned long)dest + 6)) = *((unsigned long *)((unsigned long)src + 6));
	#endif
#endif

	return true;
}


MOCKCPP_NS_END


