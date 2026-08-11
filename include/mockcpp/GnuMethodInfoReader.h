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
#ifndef __MOCKCPP_GNU_METHOD_INFO_READER_H
#define __MOCKCPP_GNU_METHOD_INFO_READER_H

#include <algorithm>
#include <stddef.h>
#include <mockcpp/mockcpp.h>

#include <mockcpp/OutputStringStream.h>
#include <mockcpp/MethodTypeTraits.h>
#include <mockcpp/ReportFailure.h>
#include <mockcpp/TypeString.h>

MOCKCPP_NS_START

///////////////////////////////////////////////////////////
struct GnuMethodDescription
{
   union {
	  void* addr;
     ptrdiff_t index;
   }u;
 
   ptrdiff_t delta;
};

inline bool gnuMethodIsVirtual(const GnuMethodDescription& method)
{
#if defined(__aarch64__) || defined(__arm__)
   // Arm's GNU C++ ABI stores the virtual bit in adj, not in ptr.
   return (method.delta & 1) != 0;
#else
   return (method.u.index & 1) != 0;
#endif
}

///////////////////////////////////////////////////////////
template <typename Method>
union MethodDescriptionUnion
{
    GnuMethodDescription desc;
    Method method;
};

///////////////////////////////////////////////////////////
template <typename Method>
void* getAddrOfMethod(Method input)
{
	MethodDescriptionUnion<Method> m;
	m.method = input;

   oss_t oss;
   oss << "Expected a non-virtual method, but "
       << TypeString<Method>::value() << " is virtual";
 
	MOCKCPP_ASSERT_TRUE(
		oss.str(),
	   !gnuMethodIsVirtual(m.desc));


   return m.desc.u.addr;
}

///////////////////////////////////////////////////////////
template <typename C, typename Method>
GnuMethodDescription getGnuDescOfVirtualMethod(Method input)
{
   typedef typename MethodTypeTraits<C, Method>::MethodType ExpectedMethodType; 
	MethodDescriptionUnion<ExpectedMethodType> m;
	m.method = input;

   oss_t oss;
   oss << "Expected a virtual method, but "
       << TypeString<Method>::value() << " is not virtual";
 
	MOCKCPP_ASSERT_TRUE(
		oss.str(),
	   gnuMethodIsVirtual(m.desc));

	return m.desc;
}

///////////////////////////////////////////////////////////
template <typename C, typename Method>
unsigned int getIndexOfMethod(Method method)
{
   const GnuMethodDescription desc =
      getGnuDescOfVirtualMethod<C, Method>(method);
#if defined(__aarch64__) || defined(__arm__)
   return static_cast<unsigned int>(
      desc.u.index/static_cast<ptrdiff_t>(sizeof(void*)));
#else
   return static_cast<unsigned int>(
      (desc.u.index - 1)/static_cast<ptrdiff_t>(sizeof(void*)));
#endif
}

///////////////////////////////////////////////////////////
template <typename C, typename Method>
unsigned int getDeltaOfMethod(Method method)
{
   const GnuMethodDescription desc =
      getGnuDescOfVirtualMethod<C, Method>(method);
#if defined(__aarch64__) || defined(__arm__)
   const ptrdiff_t adjustment = (desc.delta - 1)/2;
   return static_cast<unsigned int>(
      adjustment/static_cast<ptrdiff_t>(sizeof(void*)));
#else
   return static_cast<unsigned int>(
      desc.delta/static_cast<ptrdiff_t>(sizeof(void*)));
#endif
}

MOCKCPP_NS_END

#endif
