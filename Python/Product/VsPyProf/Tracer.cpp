// Python Tools for Visual Studio
// Copyright(c) Microsoft Corporation
// All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the License); you may not use
// this file except in compliance with the License. You may obtain a copy of the
// License at http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
// OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY
// IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
//
// See the Apache Version 2.0 License for specific language governing
// permissions and limitations under the License.

#include "stdafx.h"
#include "Tracer.h"

void
Tracer::RegisterName(
    _In_        DWORD_PTR       NameToken,
    _In_        PCWSTR          Name
)
{


}

    void
    RegisterFunction(
        _In_        DWORD_PTR       FunctionToken,
        _In_        PCWSTR          FunctionName,
        _In_        DWORD           LineNumber,
        _In_opt_    DWORD_PTR       ModuleToken,
        _In_opt_    PCWSTR          ModuleName,
        _In_opt_    PCWSTR          ModuleFilename
    );

    void
    RegisterModule(
        _In_        DWORD_PTR       ModuleToken,
        _In_        PCWSTR          ModuleName,
        _In_        PCWSTR          ModuleFilename
    );