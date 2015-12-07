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

extern "C" {

#include <Windows.h>

typedef enum _TraceEvent {
    // PyTrace_* constants.
    TraceEvent_PyTrace_CALL = 0,
    TraceEvent_PyTrace_EXCEPTION = 1,
    TraceEvent_PyTrace_LINE = 2,
    TraceEvent_PyTrace_RETURN = 3,
    TraceEvent_PyTrace_C_CALL = 4,
    TraceEvent_PyTrace_C_EXCEPTION = 5,
    TraceEvent_PyTrace_C_RETURN = 6,
} TraceEvent;

typedef struct _TRACE_EVENT {
    USHORT          Version;        //  2   2
    USHORT          EventId;        //  2   4
    DWORD           Flags;          //  4   8
    LARGE_INTEGER   SystemTime;     //  8   16
    DWORD           ProcessId;      //  4   20
    DWORD           ThreadId;       //  4   24
    DWORD_PTR       Event;          //  8   30
    USHORT          Unused;         //  2   32
} TRACE_EVENT, *PTRACE_EVENT;

typedef struct _PYTRACE_CALL {
    DWORD_PTR FrameToken;
    DWORD_PTR ModuleToken;
    DWORD_PTR FunctionToken;
    DWORD LineNumber;
} PYTRACE_CALL, *PPYTRACE_CALL;

}
