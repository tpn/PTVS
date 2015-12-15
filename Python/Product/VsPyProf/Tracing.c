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
#define _OUR_MAX_PATH MAX_UNICODE_STRING

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
        _OUR_MAX_PATH   -
        3               - /* C:\ */
        1               - // NUL
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
SetTraceStoreCriticalSection(
    _Inout_     PTRACE_STORE        TraceStore,
    _In_        PCRITICAL_SECTION   CriticalSection
)
{
    if (!TraceStore || !CriticalSection) {
        return FALSE;
    }

    TraceStore->CriticalSection = CriticalSection;

    return TRUE;
}

BOOL
InitializeStore(
    _In_        PCWSTR Path,
    _Inout_     PTRACE_STORE TraceStore,
    _In_opt_    DWORD InitialSize
)
{
    BOOL Success;

    if (!Path || !TraceStore) {
        return FALSE;
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
    _In_opt_    DWORD InitialSize
)
{
    BOOL Success;
    HRESULT Result;
    WCHAR MetadataPath[_OUR_MAX_PATH];

    if (!Path || !TraceStore) {
        return FALSE;
    }

    SecureZeroMemory(&MetadataPath, sizeof(MetadataPath));
    Result = StringCchCopyW(
        &MetadataPath[0],
        _OUR_MAX_PATH,
        Path
    );
    if (FAILED(Result)) {
        return FALSE;
    }

    Result = StringCchCatW(
        &MetadataPath[0],
        _OUR_MAX_PATH,
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
        FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (TraceStore->FileHandle == INVALID_HANDLE_VALUE) {
        goto error;
    }

    Success = InitializeStore(
        &MetadataPath[0],
        MetadataStore,
        sizeof(TRACE_STORE_METADATA)
    );

    if (!Success) {
        return FALSE;
    }

    MetadataStore->NumberOfRecords.QuadPart = 1;
    MetadataStore->RecordSize.QuadPart = sizeof(TRACE_STORE_METADATA);

    TraceStore->MetadataStore = MetadataStore;
    TraceStore->pMetadata = (PTRACE_STORE_METADATA)MetadataStore->BaseAddress;

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

PTRACE_STORES
GetMetadataStoresFromTracesStores(
    _In_    PTRACE_STORES   TraceStores
)
{
    return (PTRACE_STORES)((DWORD_PTR)TraceStores + sizeof(TRACE_STORES));
}

BOOL
InitializeTraceStores(
    _In_        PWSTR           BaseDirectory,
    _Inout_opt_ PTRACE_STORES   TraceStores,
    _Inout_     PDWORD          SizeOfTraceStores,
    _In_opt_    PDWORD          InitialFileSizes
)
{
    BOOL Success;
    HRESULT Result;
    DWORD Index;
    DWORD LastError;
    WCHAR Path[_OUR_MAX_PATH];
    LPWSTR FileNameDest;
    PTRACE_STORES MetadataStores;
    DWORD LongestFilename = GetLongestTraceStoreFileName();
    DWORD TraceStoresAllocationSize = GetTraceStoresAllocationSize();
    DWORD LongestPossibleDirectoryLength = (
        _OUR_MAX_PATH   -
        1               - // '\'
        1               - // final NUL
        LongestFilename
    );
    LARGE_INTEGER DirectoryLength;
    LARGE_INTEGER RemainingChars;
    LPDWORD Sizes = InitialFileSizes;

    if (!BaseDirectory || !SizeOfTraceStores) {
        return FALSE;
    }

    if (!TraceStores || *SizeOfTraceStores < TraceStoresAllocationSize) {
        *SizeOfTraceStores = TraceStoresAllocationSize;
        return FALSE;
    }

    MetadataStores = GetMetadataStoresFromTracesStores(TraceStores);

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
        _OUR_MAX_PATH -
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
        PTRACE_STORE MetadataStore = &MetadataStores->Stores[Index];
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
            MetadataStore,
            InitialSize
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

    if (TraceStore->CriticalSection) {
        EnterCriticalSection(TraceStore->CriticalSection);
    }

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

    if (TraceStore->CriticalSection) {
        LeaveCriticalSection(TraceStore->CriticalSection);
    }
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

    if (TraceStore->CriticalSection) {
        EnterCriticalSection(TraceStore->CriticalSection);
    }

    BytesWritten->QuadPart = (
        (DWORD_PTR)TraceStore->NextAddress -
        (DWORD_PTR)TraceStore->BaseAddress
    );

    if (TraceStore->CriticalSection) {
        LeaveCriticalSection(TraceStore->CriticalSection);
    }

    return TRUE;
}

BOOL
GetTraceStoreNumberOfRecords(
    PTRACE_STORE TraceStore,
    PULARGE_INTEGER NumberOfRecords
)
{
    if (!TraceStore) {
        return FALSE;
    }

    if (TraceStore->CriticalSection) {
        EnterCriticalSection(TraceStore->CriticalSection);
    }

    NumberOfRecords->QuadPart = TraceStore->pMetadata->NumberOfRecords.QuadPart;

    if (TraceStore->CriticalSection) {
        LeaveCriticalSection(TraceStore->CriticalSection);
    }
    return TRUE;
}

DWORD
GetTraceStoresAllocationSize(void)
{
    // Account for the metadata stores, which are located after
    // the trace stores.
    return sizeof(TRACE_STORES) * 2;
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

    if (TraceStore->CriticalSection) {
        EnterCriticalSection(TraceStore->CriticalSection);
    }

    ReturnAddress = TraceStore->NextAddress;

    TraceStore->NextAddress = (LPVOID)(
        (DWORD_PTR)ReturnAddress +
        AllocationSize
    );

    TraceStore->pMetadata->NumberOfRecords.QuadPart += NumberOfRecords.QuadPart;

    if (TraceStore->CriticalSection) {
        LeaveCriticalSection(TraceStore->CriticalSection);
    }

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
RegisterName(
    _Inout_ PTRACE_CONTEXT  TraceContext,
    _In_    DWORD_PTR       NameToken,
    _In_    PCWSTR          Name
)
{

}

VOID
RegisterFunction(
    _In_        DWORD_PTR       FunctionToken,
    _In_        PCWSTR          FunctionName,
    _In_        DWORD           LineNumber,
    _In_opt_    DWORD_PTR       ModuleToken,
    _In_opt_    PCWSTR          ModuleName,
    _In_opt_    PCWSTR          ModuleFilename
)
{

}

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
