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

#ifdef __cpp
extern "C" {
#endif

#include <Windows.h>
#include "VsPyProf.h"

typedef enum _TraceEventType {
    // PyTrace_* constants.
    TraceEventType_PyTrace_CALL = 0,
    TraceEventType_PyTrace_EXCEPTION = 1,
    TraceEventType_PyTrace_LINE = 2,
    TraceEventType_PyTrace_RETURN = 3,
    TraceEventType_PyTrace_C_CALL = 4,
    TraceEventType_PyTrace_C_EXCEPTION = 5,
    TraceEventType_PyTrace_C_RETURN = 6,
} TraceEventType;

typedef struct _EVENT_TYPE {
    TraceEventType Id;
    LPCWSTR Name;
    LPCSTR  NameA;
} EVENT_TYPE, *PEVENT_TYPE;

static EVENT_TYPE EventTypes[] = {
    { TraceEventType_PyTrace_CALL, L"PyTrace_CALL", "PyTrace_CALL" },
    { TraceEventType_PyTrace_EXCEPTION, L"PyTrace_EXCEPTION", "PyTrace_EXCEPTION" },
    { TraceEventType_PyTrace_LINE, L"PyTrace_LINE", "PyTrace_LINE" },
    { TraceEventType_PyTrace_RETURN, L"PyTrace_RETURN", "PyTrace_RETURN" },
    { TraceEventType_PyTrace_C_CALL, L"PyTrace_C_CALL", "PyTrace_C_CALL" },
    { TraceEventType_PyTrace_C_EXCEPTION, L"PyTrace_C_EXCEPTION", "PyTrace_C_EXCEPTION" },
    { TraceEventType_PyTrace_C_RETURN, L"PyTrace_C_RETURN", "PyTrace_C_RETURN" },
};

static const DWORD NumberOfTraceEventTypes = (
    sizeof(EventTypes) /
    sizeof(EVENT_TYPE)
);

typedef struct _TRACE_EVENT1 {
    USHORT          Version;        //  2   2
    USHORT          EventId;        //  2   4
    DWORD           Flags;          //  4   8
    LARGE_INTEGER   SystemTime;     //  8   16
    DWORD           ProcessId;      //  4   20
    DWORD           ThreadId;       //  4   24
    DWORD_PTR       Event;          //  8   30
    USHORT          Unused;         //  2   32
} TRACE_EVENT1, *PTRACE_EVENT1;

typedef struct _TRACE_EVENT {
    USHORT          Version;            //  2   2
    USHORT          EventType;          //  2   4
    DWORD           LineNumber;         //  4   8
    LARGE_INTEGER   SystemTime;         //  8   16
    DWORD_PTR       FramePointer;       //  8   24
    DWORD_PTR       ModulePointer;      //  8   30
    DWORD_PTR       FuncPointer;        //  8   38
    DWORD_PTR       LinePointer;        //  8   46
    DWORD_PTR       ObjPointer;         //  8   54
    USHORT          SequenceId;         //  2   56
    DWORD           ProcessId;          //  4   60
    DWORD           ThreadId;           //  4   64
} TRACE_EVENT, *PTRACE_EVENT;

typedef struct _PYTRACE_CALL {
    USHORT Version;
    DWORD LineNumber;
    USHORT Unused1;
    DWORD_PTR FrameToken;
    DWORD_PTR ModuleToken;
    DWORD_PTR FunctionToken;
    DWORD_PTR LineToken;
} PYTRACE_CALL, *PPYTRACE_CALL;

typedef struct _TRACE_STORE {
    HANDLE              FileHandle;
    HANDLE              MappingHandle;
    LARGE_INTEGER       MappingSize;
    FILE_STANDARD_INFO  FileInfo;
    LPVOID              BaseAddress;
    LPVOID              NextAddress;
    LARGE_INTEGER       RecordCount;
} TRACE_STORE, *PTRACE_STORE;

static const LPCWSTR TraceStoreFileNames[] = {
    L"trace_events.bin",
    L"trace_frames.bin",
    L"trace_modules.bin",
    L"trace_functions.bin",
    L"trace_exceptions.bin",
    L"trace_lines.bin",
};

static const DWORD NumberOfTraceStores = (
    sizeof(TraceStoreFileNames) /
    sizeof(LPCWSTR)
);

static const DWORD InitialTraceStoreFileSizes[] = {
    10 << 20,   // events
    10 << 20,   // frames
    10 << 20,   // modules
    10 << 20,   // functions
    10 << 20,   // exceptions
    10 << 20,   // lines
};

typedef struct _TRACE_STORES {
    union {
        TRACE_STORE Stores[sizeof(TraceStoreFileNames)/sizeof(LPCWSTR)];
        struct {
            TRACE_STORE Events;
            TRACE_STORE Frames;
            TRACE_STORE Modules;
            TRACE_STORE Functions;
            TRACE_STORE Exceptions;
            TRACE_STORE Lines;
        };
    };
} TRACE_STORES, *PTRACE_STORES;

VSPYPROF_API BOOL InitializeTraceStores(
    LPWSTR BaseDirectory,
    PTRACE_STORES TraceStores,
    LPDWORD InitialFileSizes
);

VSPYPROF_API void CloseTraceStore(PTRACE_STORE TraceStore);
VSPYPROF_API void CloseTraceStores(PTRACE_STORES TraceStores);

VSPYPROF_API DWORD GetTraceStoresAllocationSize(void);

VSPYPROF_API BOOL GetTraceStoreBytesWritten(
    PTRACE_STORE TraceStore,
    PLARGE_INTEGER BytesWritten
);

VSPYPROF_API BOOL GetTraceStoreRecordCount(
    PTRACE_STORE TraceStore,
    PLARGE_INTEGER RecordCount
);

VSPYPROF_API LPVOID GetNextRecord(
    PTRACE_STORE TraceStore,
    LARGE_INTEGER RecordSize
);

#ifdef __cpp
} // extern "C"
#endif
