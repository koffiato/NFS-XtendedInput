#pragma once
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

class InputConfig {
  char path[MAX_PATH] = {};
  char value[256] = {};
public:
  explicit InputConfig(const char* name = "") {
    GetModuleFileNameA((HMODULE)&__ImageBase, path, MAX_PATH - 1);
    char* end = strrchr(path, '.');
    if (end) *end = '\0';
    if (*name) {
      end = strrchr(path, '\\');
      if (end) strcpy_s(end + 1, MAX_PATH - (end + 1 - path), name);
    } else {
      strcat_s(path, ".ini");
    }
  }
  const char* GetIniPath() const { return path; }
  bool EnsureDefault(int resourceId = 101) {
    // Preserve even an empty/custom INI. Only a missing file gets fresh defaults.
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) return true;
    HMODULE module = (HMODULE)&__ImageBase;
    HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(resourceId), RT_RCDATA);
    if (!resource) return false;
    DWORD size = SizeofResource(module, resource);
    const void* bytes = LockResource(LoadResource(module, resource));
    if (!bytes || !size) return false;
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_EXISTS;
    DWORD written = 0;
    bool ok = WriteFile(file, bytes, size, &written, NULL) && written == size;
    CloseHandle(file);
    if (!ok) DeleteFileA(path);
    return ok;
  }
  const char* ReadString(const char* section, const char* key, const char* fallback) {
    GetPrivateProfileStringA(section, key, fallback, value, sizeof(value), path);
    return value;
  }
  int ReadInteger(const char* section, const char* key, int fallback) {
    return GetPrivateProfileIntA(section, key, fallback, path);
  }
  float ReadFloat(const char* section, const char* key, float fallback) {
    const char* text = ReadString(section, key, "");
    char* end;
    float result = strtof(text, &end);
    return end == text || !_finite(result) ? fallback : result;
  }
  void WriteString(const char* section, const char* key, const char* text) {
    WritePrivateProfileStringA(section, key, text, path);
  }
};
