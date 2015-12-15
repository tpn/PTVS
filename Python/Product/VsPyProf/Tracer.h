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

#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <tuple>
#include <unordered_map>
#include "PythonApi.h"
#include "Tracing.h"

using namespace std;

//typedef FileFunctionToLineNumberMap unordered_map<tuple<DWORD_PTR, DWORD_PTR>, DWORD>;

class Tracer : public ITracer {
    VsPyProf *Profiler;

    PTRACE_CONTEXT TraceContext;

    HANDLE TraceDirectory;
    HANDLE TraceFileHandle;

    unordered_map<tuple<DWORD_PTR, DWORD_PTR>, DWORD> FileFuncToLine;
    unordered_map<DWORD_PTR, LPCWSTR> Names;

    Tracer(VsPyProf *Profiler);

    Tracer& operator=(const Tracer&) { }
    Tracer(const Tracer&) { }


public:
    void
    RegisterName(
        _In_        DWORD_PTR       NameToken,
        _In_        PCWSTR          Name
    );

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
};

/*
class Tracer {
    VsPyProf *Profiler;

    HANDLE TraceDirectory;
    HANDLE TraceFileHandle;

    unordered_map<tuple<DWORD_PTR, DWORD_PTR>, DWORD> FileFuncToLine;
    unordered_map<DWORD_PTR, LPCWSTR> Names;

    Tracer(VsPyProf *Profiler);

    Tracer& operator=(const Tracer&) { }
    Tracer(const Tracer&) { }

public:
    static Tracer* Create(
        VsPyProf *Profiler
    );
    ~Tracer();

    void SourceLine(
        DWORD_PTR FunctionToken,
        DWORD_PTR FileToken,
        DWORD LineNumber
    );

    void NameToken(
        DWORD_PTR Token,
        LPCWSTR Name
    );

    void TraceCall(
        PyFrameObject *Frame,
        DWORD_PTR FunctionToken,
        DWORD_PTR ModuleToken
    );
    void TraceException(PyFrameObject *Frame, PyObject *Exception);
    void TraceLine(PyFrameObject *Frame);
    void TraceReturn(
        PyFrameObject *Frame,
        DWORD_PTR FunctionToken,
        DWORD_PTR ModuleToken,
    );
    void TraceCCall(
        PyFrameObject *Frame,
        DWORD_PTR FunctionToken,
        DWORD_PTR ModuleToken
    );

    void SourceLine(
        DWORD_PTR FunctionToken,
        DWORD_PTR FileToken,
        DWORD LineNumber
    );

    void NameToken(
        DWORD_PTR Token,
        LPCWSTR Name
    );

};
*/

