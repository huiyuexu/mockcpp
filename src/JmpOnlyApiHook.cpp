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

#include "JmpOnlyApiHook.h"
#include <mockcpp/JmpCode.h>
#include <mockcpp/CodeModifier.h>
#include <mockcpp/ReportFailure.h>

MOCKCPP_NS_START

////////////////////////////////////////////////////////
struct JmpOnlyApiHookImpl
{
   /////////////////////////////////////////////////////
   JmpOnlyApiHookImpl
              ( const void* api
              , const void* stub)
		: m_jmpCode(api, stub)
        , m_originalData(0)
        , m_api(m_jmpCode.getPatchAddress())
   {
      startHook();
   }

   /////////////////////////////////////////////////////
   ~JmpOnlyApiHookImpl()
   {
      stopHook();
   }

   /////////////////////////////////////////////////////
   void saveOriginalData()
   {
      m_originalData = new char[m_jmpCode.getCodeSize()];
      ::memcpy(m_originalData, m_api, m_jmpCode.getCodeSize());
   }

   /////////////////////////////////////////////////////
   void startHook()
   {
      saveOriginalData();
#if defined(_WIN32) || defined(_WIN64)
      // WinCodeModifier can report a cache-flush or protection-restore failure
      // after WriteProcessMemory has already installed the jump.  Preserve the
      // historical ownership behavior until its result can express that state.
      changeCode(m_jmpCode.getCodeData());
#else
      if(!changeCode(m_jmpCode.getCodeData()))
      {
         delete [] m_originalData;
         m_originalData = 0;
         MOCKCPP_REPORT_FAILURE(
            "mockcpp failed to install API hook: unable to modify target code");
      }
#endif
   }

   /////////////////////////////////////////////////////
   void stopHook()
   {
      if(m_originalData == 0)
      {
         return;
      }

      changeCode(m_originalData);
      delete [] m_originalData;
      m_originalData = 0;
   }

   /////////////////////////////////////////////////////
   bool changeCode(const void* data)
   {
      return CodeModifier::modify(m_api, data, m_jmpCode.getCodeSize());
   }

   /////////////////////////////////////////////////////
   JmpCode m_jmpCode;
   char* m_originalData;
   void* m_api;
};

/////////////////////////////////////////////////////////////////
JmpOnlyApiHook::JmpOnlyApiHook 
              ( const void* api 
              , const void* stub )
	: This(new JmpOnlyApiHookImpl(api, stub))
{
}

/////////////////////////////////////////////////////////////////
JmpOnlyApiHook::~JmpOnlyApiHook()
{
	delete This;
}

/////////////////////////////////////////////////////////////////

MOCKCPP_NS_END
