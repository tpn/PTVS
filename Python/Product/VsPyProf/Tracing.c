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

#include <Windows.h>
#include <Strsafe.h>
#include "Tracing.h"

INIT_ONCE InitOnceFindLongestTraceStoreFileName = INIT_ONCE_STATIC_INIT;

static DWORD DefaultTraceStoreCriticalSectionSpinCount = 4000;

#define MAX_UNICODE_STRING 255
#define _MAX_PATH MAX_UNICODE_STRING

BOOL
CALLBACK
FindLongestTraceStoreFileNameCallback(
    PINIT_ONCE InitOnce,
    PVOID Parameter,
    PVOID *lpContext
)
{
    DWORD Index;
    ULARGE_INTEGER Longest = { 0 };
    ULARGE_INTEGER Length = { 0 };
    DWORD MaxPath = (
        _MAX_PATH -
        3        - /* C:\ */
        1        - // NUL
        TraceStoreMetadataSuffixLength
    );

    for (Index = 0; Index < NumberOfTraceStores; Index++) {
        LPCWSTR FileName = TraceStoreFileNames[Index];
        if (FAILED(StringCchLengthW(FileName, MaxPath, &Length.QuadPart))) {
            return FALSE;
        }
        if (Length.QuadPart > Longest.QuadPart) {
            Longest.QuadPart = Length.QuadPart;
        }
    }
    Longest.QuadPart += TraceStoreMetadataSuffixLength;

    *((PDWORD64)lpContext) = Longest.QuadPart;

    return TRUE;
}

DWORD
GetLongestTraceStoreFileName()
{
    BOOL  Status;
    ULARGE_INTEGER Longest;

    Status = InitOnceExecuteOnce(
        &InitOnceFindLongestTraceStoreFileName,
        FindLongestTraceStoreFileNameCallback,
        NULL,
        (LPVOID *)&Longest.QuadPart
    );

    if (!Status || Longest.HighPart != 0) {
        return 0;
    } else {
        return Longest.LowPart;
    }
}

BOOL
RefreshTraceStoreFileInfo(PTRACE_STORE TraceStore)
{
    if (!TraceStore) {
        return FALSE;
    }

    return GetFileInformationByHandleEx(
        TraceStore->FileHandle,
        (FILE_INFO_BY_HANDLE_CLASS)FileStandardInfo,
        &TraceStore->FileInfo,
        sizeof(TraceStore->FileInfo)
    );
}

BOOL
InitializeStore(
    _In_        PCWSTR Path,
    _Inout_     PTRACE_STORE TraceStore,
    _In_opt_    DWORD InitialSize,
    _In_opt_    DWORD CriticalSectionSpinCount
)
{
    BOOL Success;
    HRESULT Result;
    DWORD SpinCount = CriticalSectionSpinCount;

    if (!Path || !TraceStore) {
        return FALSE;
    }

    if (!SpinCount) {
        SpinCount = CriticalSectionSpinCount;
    }

    if (!TraceStore->FileHandle) {
        TraceStore->FileHandle = CreateFileW(
            Path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_FLAG_OVERLAPPED,
            NULL
        );
    }

    if (TraceStore->FileHandle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    if (!RefreshTraceStoreFileInfo(TraceStore)) {
        goto error;
    }

    TraceStore->MappingSize.HighPart = 0;
    TraceStore->MappingSize.LowPart = InitialSize;

    // If the allocated size of the underlying file is less than our desired
    // mapping size (which is primed by the InitialSize parameter), extend the
    // file to that length (via SetFilePointerEx(), then SetEndOfFile()).
    if (TraceStore->FileInfo.AllocationSize.QuadPart < TraceStore->MappingSize.QuadPart) {
        LARGE_INTEGER StartOfFile = { 0 };
        Success = SetFilePointerEx(
            TraceStore->FileHandle,
            TraceStore->MappingSize,
            NULL,
            FILE_BEGIN
        );
        if (!Success) {
            goto error;
        }
        // Extend the file.
        if (!SetEndOfFile(TraceStore->FileHandle)) {
            goto error;
        }
        // Reset the file pointer back to the start.
        Success = SetFilePointerEx(
            TraceStore->FileHandle,
            StartOfFile,
            NULL,
            FILE_BEGIN
        );
        if (!Success) {
            goto error;
        }
    }

    TraceStore->MappingHandle = CreateFileMapping(
        TraceStore->FileHandle,
        NULL,
        PAGE_READWRITE,
        0,
        TraceStore->MappingSize.LowPart,
        NULL
    );

    if (TraceStore->MappingHandle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    TraceStore->BaseAddress = MapViewOfFile(
        TraceStore->MappingHandle,
        FILE_MAP_READ | FILE_MAP_WRITE,
        0,
        0,
        TraceStore->MappingSize.LowPart
    );

    if (!TraceStore->BaseAddress) {
        goto error;
    }

    TraceStore->NextAddress = TraceStore->BaseAddress;

    return TRUE;
error:
    CloseTraceStore(TraceStore);
    return FALSE;
}

BOOL
InitializeTraceStore(
    _In_        PCWSTR Path,
    _Inout_     PTRACE_STORE TraceStore,
    _Inout_     PTRACE_STORE MetadataStore,
    _In_opt_    DWORD InitialSize,
    _In_opt_    DWORD CriticalSectionSpinCount
)
{
    BOOL Success;
    HRESULT Result;
    DWORD SpinCount = CriticalSectionSpinCount;
    WCHAR MetadataPath[_MAX_PATH];

    if (!Path || !TraceStore) {
        return FALSE;
    }

    if (!SpinCount) {
        SpinCount = CriticalSectionSpinCount;
    }

    SecureZeroMemory(&MetadataPath, sizeof(MetadataPath));
    Result = StringCchCopyW(
        &MetadataPath[0],
        _MAX_PATH,
        Path
    );
    if (FAILED(Result)) {
        return FALSE;
    }

    Result = StringCchCatW(
        &MetadataPath[0],
        _MAX_PATH,
        TraceStoreMetadataSuffix
    );
    if (FAILED(Result)) {
        return FALSE;
    }

    // Create the data file first, before the :metadata stream.
    TraceStore->FileHandle = CreateFileW(
        Path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (TraceStore->FileHandle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    Success = InitializeStore(
        &MetadataPath[0],
        MetadataStore,
        sizeof(TRACE_STORE_METADATA),
        0
    );

    if (!Success) {
        return FALSE;
    }

    MetadataStore->NumberOfRecords = 1;
    MetadataStore->RecordSize = sizeof(TRACE_STORE_METADATA);

    TraceStore->MetadataStore = MetadataStore;
    TraceStore->pMetadata = (PTRACE_STORE_METADATA)MetadataStore->BaseAddress;

    Success = InitializeCriticalSectionAndSpinCount(
        &TraceStore->CriticalSection,
        SpinCount
    );
    if (!Success) {
        return FALSE;
    }

    TraceStore->MetadataHandle = CreateFileW(
        Path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_FLAG_OVERLAPPED,
        NULL
    );
    if (TraceStore->MetadataHandle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    if (!RefreshTraceStoreFileInfo(TraceStore)) {
        goto error;
    }

    TraceStore->MappingSize.HighPart = 0;
    TraceStore->MappingSize.LowPart = InitialSize;

    // If the allocated size of the underlying file is less than our desired
    // mapping size (which is primed by the InitialSize parameter), extend the
    // file to that length (via SetFilePointerEx(), then SetEndOfFile()).
    if (TraceStore->FileInfo.AllocationSize.QuadPart < TraceStore->MappingSize.QuadPart) {
        LARGE_INTEGER StartOfFile = { 0 };
        Success = SetFilePointerEx(
            TraceStore->FileHandle,
            TraceStore->MappingSize,
            NULL,
            FILE_BEGIN
        );
        if (!Success) {
            goto error;
        }
        // Extend the file.
        if (!SetEndOfFile(TraceStore->FileHandle)) {
            goto error;
        }
        // Reset the file pointer back to the start.
        Success = SetFilePointerEx(
            TraceStore->FileHandle,
            StartOfFile,
            NULL,
            FILE_BEGIN
        );
        if (!Success) {
            goto error;
        }
    }

    TraceStore->MappingHandle = CreateFileMapping(
        TraceStore->FileHandle,
        NULL,
        PAGE_READWRITE,
        0,
        TraceStore->MappingSize.LowPart,
        NULL
    );

    if (TraceStore->MappingHandle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    TraceStore->BaseAddress = MapViewOfFile(
        TraceStore->MappingHandle,
        FILE_MAP_READ | FILE_MAP_WRITE,
        0,
        0,
        TraceStore->MappingSize.LowPart
    );

    if (!TraceStore->BaseAddress) {
        goto error;
    }

    TraceStore->NextAddress = TraceStore->BaseAddress;

    return TRUE;
error:
    CloseTraceStore(TraceStore);
    return FALSE;
}

BOOL
InitializeTraceStores(
    _In_        LPWSTR          BaseDirectory,
    _Out_       PTRACE_STORES   TraceStores,
    _In_opt_    LPDWORD         InitialFileSizes,
    _In_opt_    DWORD           CriticalSectionSpinCount
)
{
    BOOL Success;
    HRESULT Result;
    DWORD Index;
    DWORD LastError;
    WCHAR Path[_MAX_PATH];
    LPWSTR FileNameDest;
    DWORD LongestFilename = GetLongestTraceStoreFileName();
    DWORD LongestPossibleDirectoryLength = (
        _MAX_PATH -
        1         - // '\'
        1         - // final NUL
        LongestFilename
    );
    LARGE_INTEGER DirectoryLength;
    LARGE_INTEGER RemainingChars;
    LPDWORD Sizes = InitialFileSizes;

    if (!BaseDirectory || !TraceStores) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!Sizes) {
        Sizes = (LPDWORD)&InitialTraceStoreFileSizes[0];
    }

    Result = StringCchLengthW(
        BaseDirectory,
        LongestPossibleDirectoryLength,
        &DirectoryLength.QuadPart
    );

    if (FAILED(Result)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (DirectoryLength.HighPart != 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    SecureZeroMemory(&Path, sizeof(Path));

    Result = StringCchCopyW(
        &Path[0],
        LongestPossibleDirectoryLength,
        BaseDirectory
    );
    if (FAILED(Result)) {
        return ERROR_INVALID_PARAMETER;
    }

    Path[DirectoryLength.LowPart] = L'\\';
    FileNameDest = &Path[DirectoryLength.LowPart+1];
    RemainingChars.QuadPart = (
        _MAX_PATH -
        DirectoryLength.LowPart -
        2
    );

    SecureZeroMemory(TraceStores, sizeof(TraceStores));

    Success = CreateDirectory(BaseDirectory, NULL);
    if (!Success) {
        LastError = GetLastError();
        if (LastError != ERROR_ALREADY_EXISTS) {
            SetLastError(LastError);
            return FALSE;
        }
    }

    for (Index = 0; Index < NumberOfTraceStores; Index++) {
        PTRACE_STORE TraceStore = &TraceStores->Stores[Index];
        LPCWSTR FileName = TraceStoreFileNames[Index];
        DWORD InitialSize = Sizes[Index];
        Result = StringCchCopyW(FileNameDest, RemainingChars.QuadPart, FileName);
        if (FAILED(Result)) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        Result = InitializeTraceStore(
            Path,
            TraceStore,
            InitialSize,
            CriticalSectionSpinCount
        );
        if (FAILED(Result)) {
            return FALSE;
        }
    }

    return TRUE;
}

VOID
CloseStore(
    _In_ PTRACE_STORE TraceStore
)
{
    if (!TraceStore) {
        return;
    }

    EnterCriticalSection(&TraceStore->CriticalSection);

    if (TraceStore->BaseAddress) {
        FlushViewOfFile(TraceStore->BaseAddress, 0);
        UnmapViewOfFile(TraceStore->BaseAddress);
        TraceStore->BaseAddress = NULL;
    }

    if (TraceStore->MappingHandle) {
        CloseHandle(TraceStore->MappingHandle);
        TraceStore->MappingHandle = NULL;
    }

    if (TraceStore->FileHandle) {
        FlushFileBuffers(TraceStore->FileHandle);
        CloseHandle(TraceStore->FileHandle);
        TraceStore->FileHandle = NULL;
    }

    DeleteCriticalSection(&TraceStore->CriticalSection);
}

VOID
CloseTraceStore(
    _In_ PTRACE_STORE TraceStore
)
{
    if (!TraceStore) {
        return;
    }

    if (TraceStore->MetadataStore) {
        ULARGE_INTEGER NumberOfRecords;
        NumberOfRecords.QuadPart = TraceStore->pNumberOfRecords->QuadPart;
        CloseStore(TraceStore->MetadataStore);
        TraceStore->MetadataStore = NULL;
    }

    CloseStore(TraceStore);
}

void
CloseTraceStores(PTRACE_STORES TraceStores)
{
    DWORD Index;

    if (!TraceStores) {
        return;
    }

    for (Index = 0; Index < NumberOfTraceStores; Index++) {
        CloseTraceStore(&TraceStores->Stores[Index]);
    }
}

BOOL
GetTraceStoreBytesWritten(
    PTRACE_STORE TraceStore,
    PULARGE_INTEGER BytesWritten
)
{
    if (!TraceStore || !TraceStore->BaseAddress || !BytesWritten) {
        return FALSE;
    }

    EnterCriticalSection(&TraceStore->CriticalSection);
    BytesWritten->QuadPart = (
        (DWORD_PTR)TraceStore->NextAddress -
        (DWORD_PTR)TraceStore->BaseAddress
    );
    LeaveCriticalSection(&TraceStore->CriticalSection);

    return TRUE;
}

BOOL
GetTraceStoreRecordCount(
    PTRACE_STORE TraceStore,
    PULARGE_INTEGER RecordCount
)
{
    if (!TraceStore) {
        return FALSE;
    }

    EnterCriticalSection(&TraceStore->CriticalSection);
    RecordCount->QuadPart = TraceStore->RecordCount.QuadPart;
    LeaveCriticalSection(&TraceStore->CriticalSection);
    return TRUE;
}

DWORD
GetTraceStoresAllocationSize(void)
{
    return sizeof(TRACE_STORES);
}

LPVOID
AllocateRecords(
    PTRACE_STORE TraceStore,
    ULARGE_INTEGER RecordSize,
    ULARGE_INTEGER NumberOfRecords
)
{
    DWORD_PTR AllocationSize;
    LPVOID ReturnAddress;

    if (!TraceStore) {
        return NULL;
    }

    if (sizeof(TraceStore->NextAddress) == sizeof(RecordSize.QuadPart)) {
        AllocationSize = (DWORD_PTR)(
            RecordSize.QuadPart *
            NumberOfRecords.QuadPart
        );
    } else {
        ULARGE_INTEGER Size = { 0 };
        // Ignore allocation attempts over 2GB on 32-bit.
        if (RecordSize.HighPart != 0 || NumberOfRecords.HighPart != 0) {
            return NULL;
        }
        Size.QuadPart = UInt32x32To64(RecordSize.LowPart, NumberOfRecords.LowPart);
        if (Size.HighPart != 0) {
            return NULL;
        }
        AllocationSize = (DWORD_PTR)Size.LowPart;
    }

    EnterCriticalSection(&TraceStore->CriticalSection);

    ReturnAddress = TraceStore->NextAddress;

    TraceStore->NextAddress = (LPVOID)(
        (DWORD_PTR)ReturnAddress +
        AllocationSize
    );

    TraceStore->pNumberOfRecords->QuadPart += NumberOfRecords.QuadPart;

    LeaveCriticalSection(&TraceStore->CriticalSection);

    return ReturnAddress;
}

LPVOID
GetNextRecord(
    PTRACE_STORE TraceStore,
    ULARGE_INTEGER RecordSize
)
{
    ULARGE_INTEGER RecordCount = { 1 };
    return AllocateRecords(TraceStore, RecordSize, RecordCount);
}

VOID
RecordName(
    _Inout_ PTRACE          TraceContext,
    _In_    DWORD_PTR       Address,
    _In_    PUNICODE_STRING String,
)
{

}

VOID
RecordModule(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    PUNICODE_STRING ModuleName,
);


VOID
RecordFunction(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       ModuleAddress,
    _In_    DWORD_PTR       FunctionAddress,
    _In_    PUNICODE_STRING FunctionName,
    _In_    LONG            LineNumber
)
{

}

#ifdef __cpp
} // extern "C"
#endif
