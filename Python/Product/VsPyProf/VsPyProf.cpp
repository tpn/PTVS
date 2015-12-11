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

// VsPyProf.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "VsPyProf.h"
#include "PythonApi.h"
#include <Windows.h>

int TraceFunction(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg) {
    return ((VsPyProfThread*)obj)->Profile(frame, what, arg);
}

extern "C" VSPYPROF_API VsPyProf* CreateProfiler(HMODULE module) {
    return VsPyProf::Create(module);
}

extern "C" VSPYPROF_API VsPyProf* CreateCustomProfiler(
    HMODULE profilerdll,
    HMODULE pythondll
)
{
    return VsPyProf::CreateCustom(profilerdll, pythondll);
}

extern "C" VSPYPROF_API void SetTracing(VsPyProf* profiler) {
    if (!profiler) {
        return;
    }
    profiler->SetTracing();
    return;
}

extern "C" VSPYPROF_API void UnsetTracing(VsPyProf* profiler) {
    if (!profiler) {
        return;
    }
    profiler->UnsetTracing();
    return;
}

extern "C" VSPYPROF_API bool IsTracing(VsPyProf* profiler) {
    if (!profiler) {
        return false;
    }
    return profiler->IsTracing();
}

extern "C" VSPYPROF_API VsPyProfThread* InitProfiler(VsPyProf* profiler) {
    if (!profiler) {
        return nullptr;
    }

    auto thread = profiler->CreateThread();

    if (thread != nullptr) {
        thread->GetProfiler()->PyEval_SetProfile(&TraceFunction, thread);
    }

    return thread;
}

extern "C" VSPYPROF_API void CloseProfiler(VsPyProf* profiler) {
    if (profiler) {
        profiler->Release();
    }
}

extern "C" VSPYPROF_API void CloseThread(VsPyProfThread* thread) {
    if (thread) {
        thread->GetProfiler()->PyEval_SetProfile(nullptr, nullptr);
        delete thread;
    }
}

// Handy for forcing a break so that Visual Studio can be attached.
extern "C" VSPYPROF_API void Debugbreak(void) {
    __debugbreak();
}

// used for compat w/ Python 2.4 where we don't have ctypes.
extern "C" VSPYPROF_API void initvspyprof(void) {
}

