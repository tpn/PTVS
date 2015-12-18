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
#include "Tracer.h"
#include <Windows.h>

int TraceFunction(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg) {
    return ((PyTraceThread*)obj)->Trace(frame, what, arg);
}

extern "C" VSPYPROF_API ITracer* CreateTracer(PTRACE_CONTEXT TraceContext, VsPyProf *Profiler)
{
    return Tracer::Create(TraceContext, Profiler);
}

/*
extern "C" VSPYPROF_API PyTraceThread* InitTracer(ITracer *tracer) {
    if (!tracer) {
        return nullptr;
    }

    return tracer->CreateThread();
}
*/

extern "C" VSPYPROF_API bool SetTracer(VsPyProf* profiler, ITracer *tracer) {
    if (!profiler || !tracer) {
        return false;
    }
    profiler->SetTracer(tracer);
    return true;
}

/*
extern "C" VSPYPROF_API bool StartTracing(VsPyProf* profiler) {
    if (!profiler || !profiler->GetTracer()) {
        return false;
    }
    return profiler->StartTracing();
}
*/

extern "C" VSPYPROF_API void CloseTracer(ITracer* tracer) {
    if (tracer) {
        //tracer->Release();
    }
}

extern "C" VSPYPROF_API void CloseTracerThread(PyTraceThread* thread) {
    if (thread) {
        thread->GetProfiler()->PyEval_SetProfile(nullptr, nullptr);
        delete thread;
    }
}
