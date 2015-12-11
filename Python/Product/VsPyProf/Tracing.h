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
    TraceEventType  Id;
    PCWSTR          Name;
    PCSTR           NameA;
} EVENT_TYPE, *PEVENT_TYPE;

static const EVENT_TYPE EventTypes[] = {
    { TraceEventType_PyTrace_CALL,          L"PyTrace_CALL",        "PyTrace_CALL" },
    { TraceEventType_PyTrace_EXCEPTION,     L"PyTrace_EXCEPTION",   "PyTrace_EXCEPTION" },
    { TraceEventType_PyTrace_LINE,          L"PyTrace_LINE",        "PyTrace_LINE" },
    { TraceEventType_PyTrace_RETURN,        L"PyTrace_RETURN",      "PyTrace_RETURN" },
    { TraceEventType_PyTrace_C_CALL,        L"PyTrace_C_CALL",      "PyTrace_C_CALL" },
    { TraceEventType_PyTrace_C_EXCEPTION,   L"PyTrace_C_EXCEPTION", "PyTrace_C_EXCEPTION" },
    { TraceEventType_PyTrace_C_RETURN,      L"PyTrace_C_RETURN",    "PyTrace_C_RETURN" },
};

static const DWORD NumberOfTraceEventTypes = (
    sizeof(EventTypes) /
    sizeof(EVENT_TYPE)
);

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;

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
    LARGE_INTEGER   FramePointer;       //  8   24
    LARGE_INTEGER   ModulePointer;      //  8   30
    LARGE_INTEGER   FuncPointer;        //  8   38
    LARGE_INTEGER   LinePointer;        //  8   46
    LARGE_INTEGER   ObjPointer;         //  8   54
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

typedef struct _TRACE_STORE_METADATA {
    ULARGE_INTEGER          NumberOfRecords;
    LARGE_INTEGER           RecordSize;
} TRACE_STORE_METADATA, *PTRACE_STORE_METADATA;

typedef struct _TRACE_STORE TRACE_STORE, *PTRACE_STORE;

typedef struct _TRACE_STORE {
    HANDLE                  FileHandle;
    HANDLE                  MappingHandle;
    LARGE_INTEGER           MappingSize;
    FILE_STANDARD_INFO      FileInfo;
    PCRITICAL_SECTION       CriticalSection;
    PVOID                   BaseAddress;
    PVOID                   NextAddress;
    PTRACE_STORE            MetadataStore;
    union {
        union {
            struct {
                ULARGE_INTEGER  NumberOfRecords;
                LARGE_INTEGER   RecordSize;
            };
            TRACE_STORE_METADATA Metadata;
        };
        PTRACE_STORE_METADATA pMetadata;
    };
} TRACE_STORE, *PTRACE_STORE;

static const LPCWSTR TraceStoreFileNames[] = {
    L"trace_events.dat",
    L"trace_frames.dat",
    L"trace_modules.dat",
    L"trace_functions.dat",
    L"trace_exceptions.dat",
    L"trace_lines.dat",
};

static const PCWSTR TraceStoreMetadataSuffix = L":metadata";
static const DWORD TraceStoreMetadataSuffixLength = (
    sizeof(TraceStoreMetadataSuffix) /
    sizeof(WCHAR)
);

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

typedef struct _TRACE_SESSION TRACE_SESSION, *PTRACE_SESSION;
typedef struct _TRACE_CONTEXT TRACE_CONTEXT, *PTRACE_CONTEXT;

typedef struct _PYTRACE_INFO {
    PVOID   FramePyObject;
    LONG    What;
    union {
        PVOID   ArgPyObject;
        PVOID   FunctionPyCodeObject;
    };
    PVOID   ModuleFilenamePyObject;
    PCWSTR  ModuleFilename;
    PCWSTR  ModuleName;
    PCWSTR  FunctionName;
    PCWSTR  Line;
    DWORD   LineNumber;
} PYTRACE_INFO, *PPYTRACE_INFO;

typedef VOID (*PRECORD_NAME_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       Address,
    _In_    PUNICODE_STRING String
);

typedef VOID (*PRECORD_MODULE_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    PUNICODE_STRING ModuleName
);

// Called for each unique (ModuleAddress, FunctionAddress, FunctionName).
typedef VOID (*PRECORD_FUNCTION_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    PUNICODE_STRING FunctionName,
    _In_    LONG            LineNumber
);
//
// Called once for each unique (ModuleAddress, ModuleName, ModulePath).
typedef VOID (*PRECORD_SOURCE_FILE_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    PUNICODE_STRING ModuleName,
    _In_    PUNICODE_STRING ModulePath
);

typedef VOID (*PRECORD_CALLBACK)(VOID);

typedef struct _RECORD_CALLBACKS {
    union {
        PRECORD_CALLBACK Callbacks[3];
        struct {
            PRECORD_NAME_CALLBACK       RecordName;
            PRECORD_MODULE_CALLBACK     RecordModule;
            PRECORD_FUNCTION_CALLBACK   RecordFunction;
        };
    };
} RECORD_CALLBACKS, *PRECORD_CALLBACKS;

typedef VOID (*PTRACE_CALL_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    DWORD_PTR       ObjectAddress,
    _In_    LONG            LineNumber
);

typedef VOID (*PTRACE_EXCEPTION_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    LONG            LineNumber,
    _In_    DWORD_PTR       ExceptionAddress
);

typedef VOID (*PTRACE_LINE_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    LONG            LineNumber
);

typedef VOID (*PTRACE_LINE_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    LONG            LineNumber
);

typedef VOID (*PTRACE_RETURN_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    LONG            LineNumber
);

typedef VOID (*PTRACE_C_CALL_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    DWORD_PTR       ObjectAddress,
    _In_    LONG            LineNumber
);

typedef VOID (*PTRACE_C_EXCEPTION_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    LONG            LineNumber,
    _In_    DWORD_PTR       ExceptionAddress
);

typedef VOID (*PTRACE_C_RETURN_CALLBACK)(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    DWORD_PTR       FrameAddress,
    _In_    LONG            LineNumber
);

typedef VOID (*PPYTRACE_CALLBACK)(VOID);

typedef struct _PYTRACE_CALLBACKS {
    union {
        PPYTRACE_CALLBACK Callbacks[7];
        struct {
            PTRACE_CALL_CALLBACK        TraceCall;
            PTRACE_EXCEPTION_CALLBACK   TraceException;
            PTRACE_LINE_CALLBACK        TraceLine;
            PTRACE_RETURN_CALLBACK      TraceReturn;
            PTRACE_C_CALL_CALLBACK      TraceCCall;
            PTRACE_C_EXCEPTION_CALLBACK TraceCException;
            PTRACE_C_RETURN_CALLBACK    TraceCReturn;
        };
    };
} PYTRACE_CALLBACKS, *PPYTRACE_CALLBACKS;

typedef BOOL (*PPYTRACE)(LPVOID Frame, INT What, LPVOID Arg);

typedef struct _TRACE_SESSION {
    LARGE_INTEGER       SessionId;
    GUID                MachineGuid;
    PISID               Sid;
    PUNICODE_STRING     UserName;
    PUNICODE_STRING     ComputerName;
    PUNICODE_STRING     DomainName;
    FILETIME            SystemTime;
} TRACE_SESSION, *PTRACE_SESSION;

typedef struct _TRACE_CONTEXT {
    TRACE_SESSION      TraceSession;
    TRACE_STORES       TraceStores;
    PYTRACE_CALLBACKS  PyTraceCallbacks;
    RECORD_CALLBACKS   RecordCallbacks;
    PPYTRACE           PyTraceFunction;
} TRACE_CONTEXT, *PTRACE_CONTEXT;

VSPYPROF_API
BOOL
InitializeTraceStores(
    _In_        PWSTR           BaseDirectory,
    _Inout_opt_ PTRACE_STORES   TraceStores,
    _Inout_     PDWORD          SizeOfTraceStores,
    _In_opt_    PDWORD          InitialFileSizes
);

VSPYPROF_API
VOID
CloseTraceStore(PTRACE_STORE TraceStore);

VSPYPROF_API
VOID
CloseTraceStores(PTRACE_STORES TraceStores);

VSPYPROF_API
DWORD
GetTraceStoresAllocationSize();

VSPYPROF_API
BOOL
GetTraceStoreBytesWritten(
    PTRACE_STORE TraceStore,
    PULARGE_INTEGER BytesWritten
);

VSPYPROF_API
BOOL
GetTraceStoreNumberOfRecords(
    PTRACE_STORE TraceStore,
    PULARGE_INTEGER NumberOfRecords
);

VSPYPROF_API
LPVOID
GetNextRecord(
    PTRACE_STORE TraceStore,
    ULARGE_INTEGER RecordSize
);

VSPYPROF_API
LPVOID
GetNextRecords(
    PTRACE_STORE TraceStore,
    ULARGE_INTEGER RecordSize,
    ULARGE_INTEGER RecordCount
);

VSPYPROF_API
VOID
RegisterName(
    _Inout_     PTRACE_CONTEXT  TraceContext,
    _In_        DWORD_PTR       NameToken,
    _In_        PCWSTR          Name
);

VSPYPROF_API
VOID
RegisterFunction(
    _Inout_     PTRACE_CONTEXT  TraceContext,
    _In_        DWORD_PTR       FunctionToken,
    _In_        PCWSTR          FunctionName,
    _In_        DWORD           LineNumber,
    _In_opt_    DWORD_PTR       ModuleToken,
    _In_opt_    PCWSTR          ModuleName,
    _In_opt_    PCWSTR          ModuleFilename
);

VSPYPROF_API
VOID
RegisterModule(
    _Inout_     PTRACE_CONTEXT  TraceContext,
    _In_        DWORD_PTR       ModuleToken,
    _In_        PCWSTR          ModuleName,
    _In_        PCWSTR          ModuleFilename
);


#ifdef __cpp
} // extern "C"
#endif
