#pragma once

#define WIN32_LEAN_AND_MEAN             // Windows ヘッダーから使用されていない部分を除外します。
// Windows ヘッダー ファイル :
#include <windows.h>
// C ランタイム ヘッダー ファイル
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#define HIDEMARU_MAIL_DIR_KEY "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\TuruKameDir"
#define FILTER_LOG_KEY "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\FilterLog"
