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

BOOL
CALLBACK
FindLongestTraceStoreFileNameCallback(
    PINIT_ONCE InitOnce,
    PVOID Parameter,
    PVOID *lpContext
)
{
    DWORD Index;
    LARGE_INTEGER Longest = { 0 };
    LARGE_INTEGER Length = { 0 };

    for (Index = 0; Index < NumberOfTraceStores; Index++) {
        LPCWSTR FileName = TraceStoreFileNames[Index];
        if (FAILED(StringCchLengthW(FileName, MAX_PATH-5, &Length.QuadPart))) {
            return FALSE;
        }
        if (Length.QuadPart > Longest.QuadPart) {
            Longest.QuadPart = Length.QuadPart;
        }
    }

    *((PDWORD64)lpContext) = Longest.QuadPart;

    return TRUE;
}

DWORD
GetLongestTraceStoreFileName()
{
    BOOL  Status;
    LARGE_INTEGER Longest;

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
InitializeTraceStore(
    LPCWSTR Path,
    PTRACE_STORE TraceStore,
    DWORD InitialSize
)
{
    BOOL Success;
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
    LPWSTR BaseDirectory,
    PTRACE_STORES TraceStores,
    LPDWORD InitialFileSizes
)
{
    BOOL Success;
    HRESULT Result;
    DWORD Index;
    DWORD LastError;
    WCHAR Path[MAX_PATH];
    LPWSTR FileNameDest;
    DWORD LongestFilename = GetLongestTraceStoreFileName();
    DWORD LongestPossibleDirectoryLength = (
        MAX_PATH -
        1        - // '\'
        1        - // final NUL
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
        MAX_PATH -
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
        Result = InitializeTraceStore(Path, TraceStore, InitialSize);
        if (FAILED(Result)) {
            return FALSE;
        }
    }

    return TRUE;
}

void
CloseTraceStore(PTRACE_STORE TraceStore)
{
    if (!TraceStore) {
        return;
    }

    if (TraceStore->BaseAddress) {
        UnmapViewOfFile(TraceStore->BaseAddress);
        TraceStore->BaseAddress = NULL;
    }

    if (TraceStore->MappingHandle) {
        CloseHandle(TraceStore->MappingHandle);
        TraceStore->MappingHandle = NULL;
    }

    if (TraceStore->FileHandle) {
        CloseHandle(TraceStore->FileHandle);
        TraceStore->FileHandle = NULL;
    }
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
    PLARGE_INTEGER BytesWritten
)
{
    if (!TraceStore || !TraceStore->BaseAddress || !BytesWritten) {
        return FALSE;
    }

    BytesWritten->QuadPart = (
        (DWORD_PTR)TraceStore->NextAddress -
        (DWORD_PTR)TraceStore->BaseAddress
    );

    return TRUE;
}

BOOL
GetTraceStoreRecordCount(
    PTRACE_STORE TraceStore,
    PLARGE_INTEGER RecordCount
)
{
    if (!TraceStore) {
        return FALSE;
    }

    RecordCount->QuadPart = TraceStore->RecordCount.QuadPart;
    return TRUE;
}

DWORD
GetTraceStoresAllocationSize(void)
{
    return sizeof(TRACE_STORES);
}

LPVOID
GetNextRecord(
    PTRACE_STORE TraceStore,
    LARGE_INTEGER RecordSize
)
{
    DWORD_PTR AllocationSize;
    LPVOID ReturnAddress;

    if (!TraceStore) {
        return NULL;
    }

    ReturnAddress = TraceStore->NextAddress;

    if (sizeof(ReturnAddress) == sizeof(RecordSize.QuadPart)) {
        AllocationSize = (DWORD_PTR)RecordSize.QuadPart;
    } else {
        // Ignore allocation attempts over 2GB on 32-bit.
        if (RecordSize.HighPart != 0) {
            return NULL;
        }
        AllocationSize = (DWORD_PTR)RecordSize.LowPart;
    }

    TraceStore->NextAddress = (LPVOID)(
        (DWORD_PTR)ReturnAddress +
        AllocationSize
    );

    TraceStore->RecordCount += 1;

    return ReturnAddress;
}

#ifdef __cpp
} // extern "C"
#endif
