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
