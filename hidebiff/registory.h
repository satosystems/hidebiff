#pragma once

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void *get_reg_value(const char *path, PDWORD type);
LONG set_reg_value(const char *path, DWORD type, CONST BYTE *pData, DWORD cbData);

#ifdef __cplusplus
}
#endif
