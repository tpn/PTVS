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

#include "stdafx.h"
#include "PythonAPI.h"
#include <strsafe.h>

HMODULE GetProfilerModule(void)
{
    wchar_t buffer[MAX_PATH];
    buffer[0] = '\0';
    static const wchar_t* const dlls[] = {
        L"\\System32\\VsPerf150.dll",
        L"\\System32\\VsPerf140.dll",
        L"\\System32\\VsPerf120.dll",
        L"\\System32\\VsPerf110.dll",
        L"\\System32\\VsPerf100.dll"
    };
    const unsigned int dll_count = sizeof(dlls) / sizeof(wchar_t);
    HMODULE mod;

    for (int i = 0; i < dll_count; i++) {
        const wchar_t *dllName = dlls[i];

        if (!GetWindowsDirectory(buffer, MAX_PATH) ||
            (wcslen(buffer) + wcslen(dllName) + 1) > MAX_PATH ||
            FAILED(StringCchCat(buffer, MAX_PATH, dllName))) {
            // should never happen
            return nullptr;
        }

        mod = LoadLibrary(buffer);
        if (mod != nullptr)
            return mod;
    }

    return nullptr;
}

VsPyProf* VsPyProf::Create(HMODULE pythonModule) {
    HMODULE profilerModule = GetProfilerModule();

    if (profilerModule == NULL)
        return nullptr;

    return CreateCustom(profilerModule, pythonModule);
}

VsPyProf* VsPyProf::CreateCustom(
    HMODULE profilerModule,
    HMODULE pythonModule
)
{
    HMODULE vsPerf = profilerModule;

    EnterFunctionFunc enterFunction = (EnterFunctionFunc)GetProcAddress(vsPerf, "EnterFunction");
    ExitFunctionFunc exitFunction = (ExitFunctionFunc)GetProcAddress(vsPerf, "ExitFunction");
    NameTokenFunc nameToken = (NameTokenFunc)GetProcAddress(vsPerf, "NameToken");
    SourceLineFunc sourceLine = (SourceLineFunc)GetProcAddress(vsPerf, "SourceLine");

    if (enterFunction == NULL || exitFunction == NULL || nameToken == NULL || sourceLine == NULL) {
        return nullptr;
    }

    auto getVersion = (GetVersionFunc*)GetProcAddress(pythonModule, "Py_GetVersion");
    auto pyCodeType = (PyObject*)GetProcAddress(pythonModule, "PyCode_Type");
    auto pyDictType = (PyObject*)GetProcAddress(pythonModule, "PyDict_Type");
    auto pyTupleType = (PyObject*)GetProcAddress(pythonModule, "PyTuple_Type");
    auto pyTypeType = (PyObject*)GetProcAddress(pythonModule, "PyType_Type");
    auto pyFuncType = (PyObject*)GetProcAddress(pythonModule, "PyFunction_Type");
    auto pyModuleType = (PyObject*)GetProcAddress(pythonModule, "PyModule_Type");
    auto pyStrType = (PyObject*)GetProcAddress(pythonModule, "PyString_Type");
    if (pyStrType == nullptr) {
        pyStrType = (PyObject*)GetProcAddress(pythonModule, "PyBytes_Type");
    }
    auto pyUniType = (PyObject*)GetProcAddress(pythonModule, "PyUnicode_Type");
    auto setProfileFunc = (PyEval_SetProfileFunc*)GetProcAddress(pythonModule, "PyEval_SetProfile");
    auto setTraceFunc = (PyEval_SetTraceFunc*)GetProcAddress(pythonModule, "PyEval_SetTrace");
    auto getItemFromStringFunc = (PyDict_GetItemString*)GetProcAddress(pythonModule, "PyDict_GetItemString");
    auto pyCFuncType = (PyObject*)GetProcAddress(pythonModule, "PyCFunction_Type");
    auto pyInstType = (PyObject*)GetProcAddress(pythonModule, "PyInstance_Type");
    auto unicodeAsUnicode = (PyUnicode_AsUnicode*)GetProcAddress(pythonModule, "PyUnicode_AsUnicode");
    auto unicodeGetLength = (PyUnicode_GetLength*)GetProcAddress(pythonModule, "PyUnicode_GetLength");

    if (getVersion != NULL && pyCodeType != NULL && pyStrType != NULL && pyUniType != NULL && setProfileFunc != NULL &&
        pyCFuncType != NULL && getItemFromStringFunc != NULL && pyDictType != NULL && pyTupleType != NULL && pyTypeType != NULL &&
        pyFuncType != NULL && pyModuleType != NULL) {
            auto version = getVersion();
            if (version != NULL) {
                // parse version like "2.7"
                int major = atoi(version);
                int minor = 0;
                while (*version && *version >= '0' && *version <= '9') {
                    version++;
                }
                if (*version == '.') {
                    version++;
                    minor = atoi(version);
                }

                if ((major == 2 && (minor >= 4 && minor <= 7)) ||
                    (major == 3 && (minor >= 0 && minor <= 5))) {
                        return new VsPyProf(pythonModule,
                            major,
                            minor,
                            enterFunction,
                            exitFunction,
                            nameToken,
                            sourceLine,
                            pyCodeType,
                            pyStrType,
                            pyUniType,
                            setProfileFunc,
                            setTraceFunc,
                            pyCFuncType,
                            getItemFromStringFunc,
                            pyDictType,
                            pyTupleType,
                            pyTypeType,
                            pyFuncType,
                            pyModuleType,
                            pyInstType,
                            unicodeAsUnicode,
                            unicodeGetLength);
                }
            }
    }

    return nullptr;
}

void VsPyProf::PyEval_SetProfile(Py_tracefunc func, PyObject* object) {
    if (_isTracing)
        _setTraceFunc(func, object);
    else
        _setProfileFunc(func, object);
}

void VsPyProf::LoadSourceFile(DWORD_PTR module, wstring& moduleName, wstring& filename)
{
    auto sourceIter = _sourceFiles.find(module);
    if (sourceIter == _sourceFiles.end()) {
        SourceFile *sf = SourceFile::Create(module, moduleName, filename);
        if (sf) {
            _sourceFiles[module] = sf;
        }
    }
}

bool VsPyProf::GetUserToken(PyFrameObject* frameObj, DWORD_PTR& func, DWORD_PTR& module) {
    auto codeObj = frameObj->f_code;
    if (codeObj->ob_type == PyCode_Type) {
        // extract func and module tokens
        PyObject *filename = nullptr;
        func = (DWORD_PTR)codeObj;

        if (PyCodeObject25_27::IsFor(MajorVersion, MinorVersion)) {
            filename = ((PyCodeObject25_27*)codeObj)->co_filename;
        } else if (PyCodeObject30_32::IsFor(MajorVersion, MinorVersion)) {
            filename = ((PyCodeObject30_32*)codeObj)->co_filename;
        } else if (PyCodeObject33_35::IsFor(MajorVersion, MinorVersion)) {
            filename = ((PyCodeObject33_35*)codeObj)->co_filename;
        }
        module = (DWORD_PTR)filename;

        // see if the function is registered
        if (_registeredObjects.find(func) == _registeredObjects.end()) {
            // get module name and register it if not already registered
            auto moduleIter = _registeredModules.find(module);

            wstring moduleName;
            if (moduleIter == _registeredModules.end()) {
                ReferenceObject(filename);

                wstring filenameStr;
                GetName(filename, filenameStr);

                // make sure we have a fully qualified path so the profiler can find our files...
                if ((filenameStr.length() >= 2 && (filenameStr[0] != '\\' || filenameStr[1] != '\\')) &&
                    (filenameStr.length() >= 3 && (filenameStr[1] != ':' || filenameStr[2] != '\\'))) {
                        // not a fully qualified path, fully qualify it.
                        wchar_t buffer[MAX_PATH];
                        if (GetCurrentDirectory(MAX_PATH, buffer) != 0) {
                            if (filenameStr[0] != '\\' && wcslen(buffer) > 0 && buffer[wcslen(buffer) - 1] != '\\') {
                                filenameStr.insert(0, L"\\");
                            }
                            filenameStr.insert(0, buffer);
                        }
                }

                GetModuleName(filenameStr, moduleName);

                _registeredModules[module] = moduleName;

                if (IsTracing()) {
                    LoadSourceFile(module, moduleName, filenameStr);
                }

                // make sure we only have valid path chars, vsperfreport doesn't like invalid chars
                for (size_t i = 0; i < filenameStr.length(); i++) {
                    if (filenameStr[i] == '<') {
                        filenameStr[i] = '(';
                    } else if (filenameStr[i] == '>') {
                        filenameStr[i] = ')';
                    } else if (filenameStr[i] == '|' ||
                        filenameStr[i] == '"' ||
                        filenameStr[i] == 124 ||
                        filenameStr[i] < 32) {
                            filenameStr[i] = '_';
                    }
                }
                _nameToken(module, filenameStr.c_str());
            } else {
                moduleName = (*moduleIter).second;
            }

            auto className = GetClassNameFromFrame(frameObj, codeObj);
            if (className.length() != 0) {
                if (moduleName.length() != 0) {
                    moduleName.append(L".");
                    moduleName.append(className);
                } else {
                    moduleName = className;
                }
            }

            ReferenceObject(codeObj);

            // register function
            _registeredObjects.insert(func);

            // associate source information
            int lineno = 0;
            if (PyCodeObject25_27::IsFor(MajorVersion, MinorVersion)) {
                RegisterName(func, ((PyCodeObject25_27*)codeObj)->co_name, &moduleName);
                lineno = ((PyCodeObject25_27*)codeObj)->co_firstlineno;
            } else if (PyCodeObject30_32::IsFor(MajorVersion, MinorVersion)) {
                RegisterName(func, ((PyCodeObject30_32*)codeObj)->co_name, &moduleName);
                lineno = ((PyCodeObject30_32*)codeObj)->co_firstlineno;
            } else if (PyCodeObject33_35::IsFor(MajorVersion, MinorVersion)) {
                RegisterName(func, ((PyCodeObject33_35*)codeObj)->co_name, &moduleName);
                lineno = ((PyCodeObject33_35*)codeObj)->co_firstlineno;
            }

            // give the profiler the line number of this function
            _sourceLine(func, module, lineno);
        }
        return true;
    }
    return false;
}

void VsPyProf::TraceLine(PyFrameObject* frameObj)
{

}

wstring VsPyProf::GetClassNameFromFrame(PyFrameObject* frameObj, PyObject *codeObj) {

    if (frameObj->f_locals != nullptr && frameObj->f_locals->ob_type == PyDict_Type) {
        // try and get self from the locals dictionary
        PyObject* self = _getItemStringFunc(frameObj->f_locals, "self");

        if (self != nullptr) {
            return GetClassNameFromSelf(self, codeObj);
        }
    } else {
        // try and get self from the fast locals if we don't have a dictionary
        int argCount;
        PyTupleObject* argNames;
        if (PyCodeObject25_27::IsFor(MajorVersion, MinorVersion)) {
            argCount = ((PyCodeObject25_27*)codeObj)->co_argcount;
            argNames = (PyTupleObject*)((PyCodeObject25_27*)codeObj)->co_varnames;
        } else if (PyCodeObject30_32::IsFor(MajorVersion, MinorVersion)) {
            argCount = ((PyCodeObject30_32*)codeObj)->co_argcount;
            argNames = (PyTupleObject*)((PyCodeObject30_32*)codeObj)->co_varnames;
        } else if (PyCodeObject33_35::IsFor(MajorVersion, MinorVersion)) {
            argCount = ((PyCodeObject33_35*)codeObj)->co_argcount;
            argNames = (PyTupleObject*)((PyCodeObject33_35*)codeObj)->co_varnames;
        }

        if (argCount != 0 && argNames->ob_type == PyTuple_Type) {
            string argName;
            GetNameAscii(argNames->ob_item[0], argName);

            if (argName == "self") {
                PyObject* self = nullptr;
                if (PyFrameObject25_33::IsFor(MajorVersion, MinorVersion)) {
                    self = ((PyFrameObject25_33*)frameObj)->f_localsplus[0];
                } else if (PyFrameObject34_35::IsFor(MajorVersion, MinorVersion)) {
                    self = ((PyFrameObject34_35*)frameObj)->f_localsplus[0];
                }
                return GetClassNameFromSelf(self, codeObj);
            }
        }
    }
    return wstring();
}

wstring VsPyProf::GetClassNameFromSelf(PyObject* self, PyObject *codeObj) {
    wstring res;
    if (self == nullptr) {
        return res;
    }

    auto mro = (PyTupleObject*)self->ob_type->tp_mro;
    if (PyInstance_Type != nullptr && self->ob_type == PyInstance_Type) {
        GetName(((PyInstanceObject*)self)->in_class->cl_name, res);
    } else if (mro != nullptr && mro->ob_type == PyTuple_Type) {
        // get the name of our code object
        string codeName;
        PyObject* nameObj = nullptr;
        if (PyCodeObject25_27::IsFor(MajorVersion, MinorVersion)) {
            nameObj = ((PyCodeObject25_27*)codeObj)->co_name;
        } else if (PyCodeObject30_32::IsFor(MajorVersion, MinorVersion)) {
            nameObj = ((PyCodeObject30_32*)codeObj)->co_name;
        } else if (PyCodeObject33_35::IsFor(MajorVersion, MinorVersion)) {
            nameObj = ((PyCodeObject33_35*)codeObj)->co_name;
        }
        GetNameAscii(nameObj, codeName);

        // walk the mro, looking for our method
        for (size_t i = 0; i < mro->ob_size; i++) {
            auto curType = (PyTypeObject*)mro->ob_item[i];

            if (curType->ob_type == PyType_Type && curType->tp_dict->ob_type == PyDict_Type) {
                auto function = _getItemStringFunc(curType->tp_dict, codeName.c_str());
                if (function != nullptr) {
                    if (function->ob_type == PyFunction_Type && ((PyFunctionObject*)function)->func_code == codeObj) {
                        // this is our method, and therefore our class!
                        // append class name onto module name.
                        auto className = curType->tp_name;
                        while (*className) {
                            res.append(1, *className);
                            className++;
                        }
                    }
                }
            }
        }
    }
    return res;
}

void VsPyProf::GetModuleName(wstring name, wstring& finalName) {
    const wchar_t* initModule = L"__init__.py";

    wstring curName = name;
    if (name.length() >= wcslen(initModule) &&
        name.compare(name.length() - wcslen(initModule), wcslen(initModule), initModule, wcslen(initModule)) == 0) {
            // it's a package, we need to remove the first __init__.py and add the package name.

            // C:\Fob\Oar\baz\__init__.py -> C, Fob\Oar\Baz, __init__, .py
            wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR], file[_MAX_FNAME];
            _wsplitpath_s(name.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, file, _MAX_FNAME, nullptr, 0);

            // Re-assemble to C:\Fob\Oar\Baz
            wchar_t newName[MAX_PATH];
            _wmakepath_s(newName, drive, dir, nullptr, nullptr);

            // C:\Fob\Oar\Baz -> C, Fob\Oar, Baz
            _wsplitpath_s(newName, drive, _MAX_DRIVE, dir, _MAX_DIR, file, _MAX_FNAME, nullptr, 0);
            finalName.append(file);	// finalName is now Baz, our package name

            // re-assemble to C:\Fob\Oar\Baz
            _wmakepath_s(newName, drive, dir, file, nullptr);

            curName = newName;
    }

    // build up name w/ any parent packages.
    for (;;) {
        wchar_t drive[_MAX_DRIVE], dir[_MAX_DIR], file[_MAX_FNAME];
        drive[0] = '\0';
        dir[0] = '\0';
        file[0] = '\0';
        _wsplitpath_s(curName.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, file, _MAX_FNAME, nullptr, 0);

        if (finalName.length() != 0) {
            finalName.insert(0, L".");
        }
        finalName.insert(0, file);

        wchar_t newName[MAX_PATH];
        newName[0] = '\0';
        _wmakepath_s(newName, drive, dir, L"__init__.py", nullptr);

        if (GetFileAttributes(newName) == -1) {
            // finalName is it
            return;
        } else {
            // go up one level
            curName.clear();
            curName.append(drive);
            curName.append(dir, wcslen(dir) - 1);
        }
    }
}

bool VsPyProf::GetBuiltinToken(PyObject* codeObj, DWORD_PTR& func, DWORD_PTR& module) {
    if (codeObj->ob_type == PyCFunction_Type) {
        func = (DWORD_PTR)((PyCFunctionObject*)codeObj)->m_ml;
        module = (DWORD_PTR)((PyCFunctionObject*)codeObj)->m_module;

        PyObject *modulePyObj;
        if (module == NULL) {
            if (((PyCFunctionObject*)codeObj)->m_self != nullptr) {
                // bound instance method such as str.startswith
                module = (DWORD_PTR)((PyCFunctionObject*)codeObj)->m_self->ob_type->tp_name;
                modulePyObj = ((PyCFunctionObject*)codeObj)->m_self->ob_type;
            } else {
                module = (DWORD_PTR)"Unknown Module";
                modulePyObj = nullptr;
            }
        } else {
            modulePyObj = ((PyCFunctionObject*)codeObj)->m_module;
        }

        if (_registeredObjects.find(func) == _registeredObjects.end()) {

            _registeredObjects.insert(func);
            ReferenceObject(codeObj);	 // keep alive the method def via the code object


            wstring name, moduleName;

            if (modulePyObj != nullptr && GetName(modulePyObj, moduleName) && moduleName.length() != 0 &&
                ((MajorVersion == 2 && moduleName != L"__builtin__") || (MajorVersion == 3 && moduleName != L"builtins"))) {
                    name.append(moduleName);
                    name.append(L".");
            }

            if (((PyCFunctionObject*)codeObj)->m_self != nullptr) {
                auto type = ((PyCFunctionObject*)codeObj)->m_self->ob_type;

                // In Python3k module methods apparently have the module as their self, modules don't 
                // actually have any interesting methods so we can always filter.
                if (type != PyModule_Type) {
                    auto className = type->tp_name;

                    for (int i = 0; className[i]; i++) {
                        name.append(1, className[i]);
                    }
                    name.append(L".");
                }
            }

            char* method_name = ((PyCFunctionObject*)codeObj)->m_ml->ml_name;
            for (int i = 0; method_name[i]; i++) {
                name.append(1, method_name[i]);
            }

            _nameToken(func, name.c_str());

            auto moduleIter = _registeredObjects.find(module);
            if (moduleIter == _registeredObjects.end()) {
                if (modulePyObj != nullptr) {
                    ReferenceObject(modulePyObj);
                }

                _registeredObjects.insert(module);
                if (modulePyObj != nullptr) {
                    RegisterName(module, modulePyObj);
                } else {
                    _nameToken(module, L"Unknown Module");
                }
            }
        }
        return true;
    }
    return false;
}


void VsPyProf::RegisterName(DWORD_PTR token, PyObject* nameObj, wstring* moduleName) {
    // register function
    wstring name;
    GetName((PyObject*)nameObj, name);

    if (name.compare(L"<module>") == 0) {
        name.clear();
        if (moduleName != nullptr) {
            name.append(*moduleName);
            name.append(L" (module)");
        }
    } else if (moduleName != nullptr) {
        name.insert(0, L".");
        name.insert(0, *moduleName);
    }
    _nameToken(token, name.c_str());
}

bool VsPyProf::GetName(PyObject* object, wstring& name) {
    if (object->ob_type == PyStr_Type) {
        auto str = (PyStringObject*)object;
        for (size_t i = 0; i < str->ob_size; i++) {
            name.append(1, (wchar_t)str->ob_sval[i]);
        }
    } else if (object->ob_type == PyUni_Type) {
        if (MajorVersion == 3 && MinorVersion > 2) {
            name.append(_asUnicode(object), _unicodeGetLength(object));
        } else {
            auto uni = (PyUnicodeObject*)object;
            name.append(uni->str, uni->length);
        }
    } else {
        name.append(L"Unidentifiable Method");
        return false;
    }
    return true;
}

void VsPyProf::GetNameAscii(PyObject* object, string& name) {
    if (object->ob_type == PyStr_Type) {
        auto str = (PyStringObject*)object;
        name.append(str->ob_sval, str->ob_size);
    } else if (object->ob_type == PyUni_Type) {
        if (MajorVersion == 3 && MinorVersion > 2) {
            size_t length = _unicodeGetLength(object);
            wchar_t* value = _asUnicode(object);
            for (size_t i = 0; i < length; i++) {
                name.append(1, (char)value[i]);
            }
        } else {
            auto uni = (PyUnicodeObject*)object;
            for (size_t i = 0; i < uni->length; i++) {
                name.append(1, (char)uni->str[i]);
            }
        }
    } else {
        name.append("Unidentifiable Method");
    }
}

VsPyProf::VsPyProf(HMODULE pythonModule, int majorVersion, int minorVersion, EnterFunctionFunc enterFunction, ExitFunctionFunc exitFunction, NameTokenFunc nameToken, SourceLineFunc sourceLine, PyObject* pyCodeType, PyObject* pyStringType, PyObject* pyUnicodeType, PyEval_SetProfileFunc* setProfileFunc, PyEval_SetTraceFunc* setTraceFunc, PyObject* cfunctionType, PyDict_GetItemString* getItemStringFunc, PyObject* pyDictType, PyObject* pyTupleType, PyObject* pyTypeType, PyObject* pyFuncType, PyObject* pyModuleType, PyObject* pyInstType, PyUnicode_AsUnicode* asUnicode, PyUnicode_GetLength* unicodeGetLength)
    : _pythonModule(pythonModule),
    MajorVersion(majorVersion),
    MinorVersion(minorVersion),
    _enterFunction(enterFunction),
    _exitFunction(exitFunction),
    _nameToken(nameToken),
    _sourceLine(sourceLine),
    PyCode_Type(pyCodeType),
    PyStr_Type(pyStringType),
    PyUni_Type(pyUnicodeType),
    _setProfileFunc(setProfileFunc),
    _setTraceFunc(setTraceFunc),
    PyCFunction_Type(cfunctionType),
    _getItemStringFunc(getItemStringFunc),
    PyDict_Type(pyDictType),
    PyTuple_Type(pyTupleType),
    PyType_Type(pyTypeType),
    PyFunction_Type(pyFuncType),
    PyModule_Type(pyModuleType),
    PyInstance_Type(pyInstType),
    _asUnicode(asUnicode),
    _unicodeGetLength(unicodeGetLength) {
}

VsPyProfThread::VsPyProfThread(VsPyProf* profiler) : _profiler(profiler), _depth(0) {
    profiler->AddRef();
    ob_refcnt = 1;
}

VsPyProfThread::~VsPyProfThread() {
    _profiler->Release();
}

VsPyProf* VsPyProfThread::GetProfiler() {
    return _profiler;
}

int VsPyProfThread::Trace(PyFrameObject *frame, int what, PyObject *arg) {
    DWORD_PTR func, module;

    switch (what) {
    case PyTrace_LINE:
        if (_profiler->_isTracing && _profiler->GetUserToken(frame, func, module)) {
            _profiler->TraceLine(frame);
        }
        break;
    case PyTrace_CALL:
        if (++_depth > _skippedFrames && _profiler->GetUserToken(frame, func, module)) {
            _profiler->_enterFunction(func, module);
        }
        break;
    case PyTrace_RETURN:
        if (_depth && --_depth > _skippedFrames && _profiler->GetUserToken(frame, func, module)) {
            _profiler->_exitFunction(func, module);
        }
        break;
    case PyTrace_C_CALL:
        if (++_depth > _skippedFrames && _profiler->GetBuiltinToken(arg, func, module)) {
            _profiler->_enterFunction(func, module);
        }
        break;
    case PyTrace_C_RETURN:
        if (_depth && --_depth > _skippedFrames && _profiler->GetBuiltinToken(arg, func, module)) {
            _profiler->_exitFunction(func, module);
        }
        break;
    }

    return 0;
}
