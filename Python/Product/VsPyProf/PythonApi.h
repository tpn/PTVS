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

#ifndef __PYTHONAPI_H__
#define __PYTHONAPI_H__

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "python.h"
#include "VSPerf.h"
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <string>
#include <strsafe.h>
#include "SourceFile.h"

using namespace std;

typedef void PyEval_SetProfileFunc(Py_tracefunc func, PyObject *obj);
typedef void PyEval_SetTraceFunc(Py_tracefunc func, PyObject *obj);
typedef PyObject* PyDict_GetItemString(PyObject *dp, const char *key);

// function definitions so we can dynamically link to profiler APIs
// turns name into token
typedef PROFILE_COMMAND_STATUS (_stdcall *NameTokenFunc)(DWORD_PTR token, const wchar_t* name);
// reports line number of a function
typedef PROFILE_COMMAND_STATUS (_stdcall *SourceLineFunc)(DWORD_PTR functionToken, DWORD_PTR fileToken, unsigned int lineNumber);
// notify function entry
typedef void (__stdcall *EnterFunctionFunc)(DWORD_PTR FunctionToken, DWORD_PTR ModuleToken);
// notify function exit
typedef void (__stdcall *ExitFunctionFunc)(DWORD_PTR FunctionToken, DWORD_PTR ModuleToken);

typedef struct _VSPERF {
    HANDLE ModuleHandle;
    NameTokenFunc NameToken;
    SourceLineFunc SourceLine;
    EnterFunctionFunc EnterFunction;
    ExitFunctionFunc ExitFunction;
} VSPERF, *PVSPERF;

typedef wchar_t* PyUnicode_AsUnicode(PyObject *unicode           /* Unicode object */);
typedef size_t PyUnicode_GetLength(PyObject *unicode);
typedef int PyFrame_GetLineNumber(PyFrameObject *f);

class PyTraceInfo {
public:
    union {
        PyFrameObject   *FrameObject;
        DWORD_PTR        FrameToken;
    };
    int What;
    union {
        PyObject        *ArgObject;
        PyObject        *CodeObject;
        DWORD_PTR        FunctionToken;
    };
    PCWSTR FunctionName;
    PCWSTR Line;
    DWORD  LineNumber;
    union {
        PyObject        *ModuleFilenameObject;
        DWORD_PTR        ModuleToken;
    };
    PCWSTR ModuleName;
    PCWSTR ModuleFilename;
};

class VsPyProf;

class ITracer {
public:

    virtual void Trace(_In_ PyTraceInfo *TraceInfo) = 0;

    virtual void RegisterName(
        _In_        DWORD_PTR       NameToken,
        _In_        PCWSTR          Name
    ) = 0;

    virtual void RegisterFunction(
        _In_        DWORD_PTR       FunctionToken,
        _In_        PCWSTR          FunctionName,
        _In_        DWORD           LineNumber,
        _In_opt_    DWORD_PTR       ModuleToken,
        _In_opt_    PCWSTR          ModuleName,
        _In_opt_    PCWSTR          ModuleFilename
    ) = 0;

    virtual void RegisterModule(
        _In_        DWORD_PTR       ModuleToken,
        _In_        PCWSTR          ModuleName,
        _In_        PCWSTR          ModuleFilename
    ) = 0;

    virtual bool StartTracing() = 0;
    virtual bool IsTracing() = 0;
    virtual bool StopTracing() = 0;
};

class VsPyProfThread : public PyObject {
    VsPyProf* _profiler;
    int _depth;
    static const int _skippedFrames = 1;
public:
    VsPyProfThread(VsPyProf* profiler);
    ~VsPyProfThread();
    VsPyProf* GetProfiler();

    bool IsTracing();

    int Profile(PyFrameObject *frame, int what, PyObject *arg);
    int Trace(PyFrameObject *frame, int what, PyObject *arg);
};

class PyTraceThread : public VsPyProfThread {
    ITracer *_tracer;
public:
    PyTraceThread(VsPyProf* profiler);
    ~PyTraceThread();
    ITracer *GetTracer() { return _tracer; };

    int Trace(PyFrameObject *frame, int what, PyObject *arg);
};


// Implements Python profiling.  Supports multiple Python versions (2.4 - 3.4) simultaneously.
// This code is always called w/ the GIL held (either from a ctypes call where we're a PyDll or
// from the runtime for our trace func).
class VsPyProf {
    friend class VsPyProfThread;
    //friend class Tracer;

    ITracer *_tracer;
    bool _isTracing;
    DWORD _processId;
    DWORD _threadId;

    HMODULE _profileModule;
    HMODULE _pythonModule;
    PyEval_SetProfileFunc* _setProfileFunc;
    PyEval_SetTraceFunc* _setTraceFunc;
    PyDict_GetItemString* _getItemStringFunc;
    PyUnicode_AsUnicode* _asUnicode;
    PyUnicode_GetLength* _unicodeGetLength;
    PyFrame_GetLineNumber* _frameGetLineNumber;

    unordered_map<DWORD_PTR, PyTraceInfo> _traceInfo;

    unordered_set<DWORD_PTR> _registeredObjects; // functions
    unordered_set<PyObject*> _referencedObjects;
    unordered_map<DWORD_PTR, wstring> _registeredModules;
    unordered_map<DWORD_PTR, SourceFile*> _sourceFiles; // function token <-> Source File
    unordered_map<DWORD_PTR, wstring> _names;
    unordered_map<DWORD_PTR, wstring> _filenames;

    // Python type objects
    PyObject* PyCode_Type;
    PyObject* PyStr_Type;
    PyObject* PyUni_Type;
    PyObject* PyCFunction_Type;
    PyObject* PyDict_Type;
    PyObject* PyTuple_Type;
    PyObject* PyType_Type;
    PyObject* PyFunction_Type;
    PyObject* PyModule_Type;
    PyObject* PyInstance_Type;

    VSPERF vsperf;

    // Profiler APIs.  If we're profiling, and not tracing,
    // these will be set to their VsPerf counterparts in the
    // above structure.
    EnterFunctionFunc _enterFunction;
    ExitFunctionFunc _exitFunction;
    NameTokenFunc _nameToken;
    SourceLineFunc _sourceLine;
    //TraceLineFunc _traceLine;

    VsPyProf(HMODULE pythonModule, int majorVersion, int minorVersion, PVSPERF vsperf, PyObject* pyCodeType, PyObject* pyStringType, PyObject* pyUnicodeType, PyEval_SetProfileFunc* setProfileFunc, PyEval_SetTraceFunc* setTraceFunc, PyObject* cfunctionType, PyDict_GetItemString* getItemStringFunc, PyObject* pyDictType, PyObject* pyTupleType, PyObject* pyTypeType, PyObject* pyFuncType, PyObject* pyModuleType, PyObject* pyInstType, PyUnicode_AsUnicode* asUnicode, PyUnicode_GetLength* unicodeGetLength, PyFrame_GetLineNumber* frameGetLineNumber);

    // Extracts the function and module identifier from a user defined function
    bool GetUserToken(PyFrameObject *frame, DWORD_PTR& func, DWORD_PTR& module);
    bool GetBuiltinToken(PyObject* codeObj, DWORD_PTR& func, DWORD_PTR& module);
    void GetModuleName(wstring module_name, wstring& finalName);

    bool TraceUserFunction(PyFrameObject *frame, PyTraceInfo *info);
    bool GetBuiltinToken(PyFrameObject *frame, PyTraceInfo *info);

    bool GetModuleFilenameFromCodeObject(PyObject *codeObj, PyObject **filename);
    static void EnsureQualifiedPath(wstring& filenameStr);
    static void NormalizePathForVsPerfReport(wstring& filenameStr);
    static void FixupModuleNameFromClassName(wstring &className, wstring &moduleName);

    bool GetFunctionNameObjectAndLineNumberFromCodeObject(PyObject *codeObj, PyObject **funcname, PDWORD lineno);

    bool RegisterFunction(
        DWORD_PTR func,
        DWORD_PTR module,
        PyObject *codeObj,
        wstring &moduleName,
        wstring &moduleFilename,
        PDWORD lineno
    );

    wstring GetClassNameFromSelf(PyObject* self, PyObject *codeObj);
    wstring GetClassNameFromFrame(PyFrameObject* frameObj, PyObject *codeObj);

    void RegisterName(DWORD_PTR token, PyObject* name, wstring* moduleName, _Out_opt_ PCWSTR *funcname);
    bool GetName(PyObject* object, wstring& name);
    void GetNameAscii(PyObject* object, string& name);

    void TraceLine(PyFrameObject *frame);
    void LoadSourceFile(DWORD_PTR module, wstring& moduleName, wstring& filename);

    void ReferenceObject(PyObject* object) {
        object->ob_refcnt++;
        _referencedObjects.insert(object);
    }

    int MajorVersion, MinorVersion, _refCount;

public:
    // Creates a new instance of the PythonApi from the given DLL.  Returns null if the
    // version is unsupported or another error occurs.
    static VsPyProf* Create(HMODULE pythonModule);
    static VsPyProf* CreateCustom(
        HMODULE profileModule,
        HMODULE pythonModule
    );

    void PyEval_SetProfile(Py_tracefunc func, PyObject* object);
    VsPyProfThread* CreateThread() {
        return new VsPyProfThread(this);
    }

    void SetTracer(ITracer *tracer) {
        _tracer = tracer;
    };
    ITracer *GetTracer(void) { return _tracer; }
    void UnsetTracing(void) { _isTracing = false; }
    bool IsTracing(void) { return _isTracing; }

    void AddRef() {
        _refCount++;
    }

    void Release() {
        if(--_refCount == 0) {
            delete this;
        }
    }

    ~VsPyProf() {
        // release all objects we hold onto...
        for(auto cur = _referencedObjects.begin(); cur != _referencedObjects.end(); cur++) {
            if(--(*cur)->ob_refcnt == 0) {
                (*cur)->ob_type->tp_dealloc(*cur);
            }
        }

        for (auto psf = _sourceFiles.begin(); psf != _sourceFiles.end(); psf++) {
            delete (*psf).second;
        }
    }

};


#endif
