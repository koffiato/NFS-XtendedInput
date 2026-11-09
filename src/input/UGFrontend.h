// UG frontend callbacks used by the input hooks. MIT; see LICENSE.
#pragma once
#include "../_vs/stdafx.h"

int __stdcall FEPrintf(void* FEString, const char* format);
int __stdcall FEngFindString(const char* pkgname, int hash);
void SnoopLastFEPackage(char* format, char* pkg);
void FEngGlobalCave();
