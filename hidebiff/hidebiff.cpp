/*
 * Copyright (c) 2006-2012 Satoshi Ogata
 */

/*
【デバッグの方法】
★受信したメールをもう一度トースト表示したい場合
1. 受信したメールのアカウントの ini ファイルの
   LogTime の数値を適当に減らし、LogCount を 1 つ戻す。
2. デバッグ起動のオプションに以下を指定する
    -c -r 1
   最後の数字がさかのぼってトースト表示させたいメールの数。
*/

/**
 * @file
 * アプリケーションのエントリ ポイントが定義されているファイル。
 */

#include <windows.h>
#include <psapi.h> // EnumProcesses
#include <shlwapi.h> // PathFileExsits
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <map>
#include <boost/regex.hpp>
#include "hidebiff.h"
#include "IniFile.h"
#include "resource.h"
#include "registory.h"

HANDLE IniFile::hMutex = NULL;

using namespace std;

#define DELIM { 0x0c, '!', ' ', 'e', ':', '\0' } ///< ログファイルのデリミタ。
#define DELIM2 { 0x0c, '!', ' ', 'r', ':', '\0' } ///< デコード済みログファイルのデリミタ。

/**
 * ファイルにデバッグログ出力を行うか否か。
 *
 * デバッグビルド構成、または --debug を引数に加えた場合に true になる。
 */
static bool Debug = false;

/**
 * このプロセスのインスタンス変数。
 */
static HINSTANCE hInst;

static const char *getWorkDir(void);

/**
 * デバッグ出力を行なう。
 *
 * 引数 msg が NULL なら何も行なわない。
 * msg が NULL ではなく、デバッグ出力用ファイルポインタ #debugout が NULL なら
 * #debugout を初期化する。
 *
 * デバッグ出力を行なった場合、プログラム終了時に
 * #debugout をクローズしなければならない。
 *
 * @param [in] msg デバッグ出力メッセージ
 */
static FILE *debugout = NULL;
#define DEBUG_OUTPUT "hidebiff.log" ///< デバッグ出力ファイルパス。
#if _DEBUG
/**
 * デバッグ出力を行う。
 *
 * @param [in] msg デバッグメッセージ
 */
static void debug(const wchar_t *msg) {
	if (!Debug) return;
	string path = getWorkDir();
	path += "\\";
	path += DEBUG_OUTPUT;
	if (msg) {
		if (debugout == NULL) {
			fopen_s(&debugout, path.c_str(), "a");
		}
		fwprintf(debugout, L"%s", msg);
	}
}

/**
 * デバッグ出力を行う。
 *
 * @param [in] msg デバッグメッセージ
 */
static void debug(wstring msg) {
	debug(msg.c_str());
}

/**
 * デバッグ出力を行う。
 *
 * @param [in] msg デバッグメッセージ
 */
static void debug(const char *msg) {
	static wchar_t dest[1024];
	size_t num;
	mbstowcs_s(&num, dest, msg, sizeof(dest));
	debug(wstring(dest));
}

/**
 * デバッグ出力を行う。
 *
 * @param [in] msg デバッグメッセージ
 */
static void debug(string msg) {
	debug(msg.c_str());
}

/**
 * デバッグ出力を行う。
 *
 * @param [in] i デバッグメッセージ
 */
static void debug(int i) {
	char buf[20] = {0};
	_itoa_s(i, buf, 10);
	debug(buf);
}

/**
 * デバッグ出力を行う。
 *
 * @param [in] i デバッグメッセージ
 */
static void debug(size_t i) {
	debug(static_cast<int>(i));
}
#else
#define debug(a)
#endif

/**
 * 16 進数の 8 桁の文字列を 64 bit DWORD にして返す。
 * フォーマットが不適切な場合は 0 を返す。
 *
 * @param [in] hex 16 進数文字列
 * @return 数値
 */
static DWORD hexatoint64(char *hex) {
	int result = 0;
	for (int i = 0; i < 8; i++) {
		if (hex[i] >= 'A' && hex[i] <= 'F') {
			result += (hex[i] - 'A' + 10) << ((8 - i - 1) * 4);
		} else if (hex[i] >= 'a' && hex[i] <= 'f') {
			result += (hex[i] - 'a' + 10) << ((8 - i - 1) * 4);
		} else if (hex[i] >= '0' && hex[i] <= '9') {
			result += (hex[i] - '0') << ((8 - i - 1) * 4);
		} else {
			result = 0;
			break;
		}
	}
	return result;
}

/**
 * エラーメッセージを表示する。
 *
 * 直前に発生したエラーメッセージをメッセージボックスに表示する。
 * エラーが発生しなかった場合は、処置完了メッセージを表示する。
 *
 * @param [in] title メッセージボックスのタイトル
 * @param [in] msgid メッセージ ID
 */
static void msgbox(const char *title, DWORD msgid = 0, const char *append = NULL) {
	LPSTR lpBuffer;
	if (msgid == 0) msgid = GetLastError();
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,	// 入力元と処理方法のオプション
		NULL,															// メッセージの入力元
		msgid,															// メッセージ識別子
		LANG_USER_DEFAULT,												// 言語識別子
		(LPSTR)&lpBuffer,												// メッセージバッファ
		0,																// メッセージバッファの最大サイズ
		NULL															// 複数のメッセージ挿入シーケンスからなる配列
	);
	if (title == NULL) {
		debug(lpBuffer);
		debug("\n");
	} else if (append != NULL) {
		size_t len = strlen(append) + strlen(lpBuffer) + 2;
		LPSTR buff = (LPSTR)malloc(len);
		strcpy_s(buff, len, lpBuffer);
		strcat_s(buff, len, append);
		MessageBox(NULL, buff, title, MB_ICONHAND | MB_OK);
		free(buff);
	} else {
		MessageBox(NULL, lpBuffer, title, MB_ICONHAND | MB_OK);
	}
	LocalFree(lpBuffer);
}

/**
 * 指定されたファイルの内容を取得する。
 *
 * 呼び出し元で戻り値のポインタを開放しなければならない。
 * size が NULL の場合は、当然ファイルサイズはポインタに出力されない。
 *
 * この関数は [ファイル内容] + ['\0'] という形式でポインタを返す。
 * ただし出力される size は最後の \0 を含まない。
 *
 * ファイルが存在しない場合、処理に失敗した場合は NULL を返す。
 *
 * @param [in] filename ファイルパス
 * @param [out] size ファイルサイズ
 * @return ファイルの内容
 */
static char *getFileContents(const char *filename, DWORD *size) {
	HANDLE file1;
	HANDLE map1;
	DWORD hsize1, lsize1;
	char *view1;
	char *result = NULL;
	static int count = 0;
	string map("filemapping");
	char buf[6] = {0};
	_itoa_s(count++, buf, 10);
	map += buf;

	// ファイルの生成
	file1 = CreateFile(filename, GENERIC_READ | FILE_SHARE_READ | FILE_SHARE_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
	if (file1 == INVALID_HANDLE_VALUE) {
		// ファイルが存在しない
		char append[1024];
		sprintf_s(append, "\nPath:%s", filename);
		msgbox("error 12", 0, append);
		return result;
	}

	// サイズの取得
	lsize1 = GetFileSize(file1, &hsize1);
	if (hsize1 != 0) {
		// ファイルが大きすぎる場合はエラーにしちゃう
		msgbox("error 22");
		return result;
	}
	
	// メモリマップドファイルの生成
	map1 = CreateFileMapping(file1, NULL, PAGE_READONLY, hsize1, lsize1, map.c_str());
	if (map1 == NULL) {
		// エラー処理
		msgbox("error 32");
		CloseHandle(file1);
		return result;
	}
	
	// マップビューの生成
	view1 = (char *)MapViewOfFile(map1, FILE_MAP_READ, 0, 0, 0);
	if (view1 == NULL) {
		// エラー処理
		msgbox("error 42");
		CloseHandle(map1);
		CloseHandle(file1);
		return result;
	}
	
	result = (char *)malloc(lsize1 + 1);
	if (result) {
		memcpy(result, view1, lsize1);
		result[lsize1] = '\0';
	}
	
	// アンマップ
	if (!UnmapViewOfFile(view1)) {
		// エラー処理
		msgbox("error 5s");
	}

	CloseHandle(map1);
	CloseHandle(file1);

	if (size != NULL) {
		*size = lsize1;
	}

	return result;
}

/**
 * INI ファイルを保存するディレクトリ（このモジュールが配置されているディレクトリ）を取得する。
 *
 * この関数はこの関数内の static 変数のポインタを返すため、
 * 呼び出し元はポインタの指す先を変更してはならない。
 *
 * @return ディレクトリパス
 */
static const char *getWorkDir(void) {
	static char workDir[MAX_PATH + 1] = {'\0'};
	if (workDir[0] == '\0') {
		const char *hidemaruver = "HKEY_CURRENT_USER\\Software\\Hidemaruo\\Hidemaru\\RegVer";
		const char *macropath = "HKEY_CURRENT_USER\\Software\\Hidemaruo\\Hidemaru\\Env\\MacroPath";
		const char *turukamedir = HIDEMARU_MAIL_DIR_KEY;
		const char *hideinstloc = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Hidemaru\\InstallLocation";
		char *work;
		void *temp;
		DWORD type;
		
		if ((temp = get_reg_value(hidemaruver, &type)) == NULL) {
			// 秀丸がインストールされていないなら、秀丸メールディレクトリがホーム
			work = (char *)get_reg_value(turukamedir, &type);
		} else {
			// 秀丸がインストールされているならマクロパスを調べる
			work = (char *)get_reg_value(macropath, &type);
			// 設定がブランクなら秀丸の InstallLocation がホーム
			if (work != NULL && work[0] == '\0') {
				free(work);
				work = NULL;
			}
			if (work == NULL) {
				// マクロパスが存在しなければ秀丸の InstallLocation がホーム
				work = (char *)get_reg_value(hideinstloc, &type);
			}
		}
		if (temp != NULL) {
			free(temp);
		}
		strcpy_s(workDir, work);
		free(work);
	}
	return workDir;
}

/**
 * メールディレクトリを取得する。
 *
 * この関数はこの関数内の static 変数のポインタを返すため、
 * 呼び出し元はポインタの指す先を変更してはならない。
 *
 * @return メールディレクトリ。文字列の最後に "\" が付与していることに注意
 */
static const char *getMailDir(void) {
	static char mailDir[MAX_PATH + 1] = {'\0'};
	const char *MAIL_DIR = "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\HomeDir";

	if (mailDir[0] == '\0') {
		DWORD type;
		// メールデータ用のディレクトリを取得
		char *work = (char *)get_reg_value(MAIL_DIR, &type);
		strcpy_s(mailDir, work);
		free(work);
	}
	
	return mailDir;
}


/**
 * メールアカウントを取得する。
 *
 * 戻り値は適切に開放しなければならない。
 *
 * @param [in] mailDir メールディレクトリ
 * @param [out] メールアカウント数
 * @return メールアカウントのディレクトリ名
 */
static char** getMailAccountDirs(const char *mailDir, int *count) {
	string subdir_filename(mailDir);
	subdir_filename.append("subdir.bin");
	vector<string> dirs;
	char **subdirs;
	char *contents = getFileContents(subdir_filename.c_str(), NULL);
	char *ctx;
	const char *delim = "\r\n";
	char *next = strtok_s(contents, delim, &ctx);
	while (next) {
		if (next[0] != '\\') {
			dirs.push_back(string(next));
		}
		next = strtok_s(NULL, delim, &ctx);
	}
	free(contents);
	*count = (int)dirs.size();
	if (*count) {
		subdirs = (char **)malloc(sizeof(char *) * *count);
		for (int i = 0; i < *count; i++) {
			subdirs[i] = (char *)malloc(dirs[i].length() + 1);
			strcpy_s(subdirs[i], dirs[i].length() + 1,dirs[i].c_str());
		}
	} else {
		subdirs = NULL;
	}
	return subdirs;
}

/**
 * ログファイルを検索する。
 * 
 * @param [in] account アカウント名
 * @param [in] dirName ログファイルを探すディレクトリ
 * @param [out] logfiles 見つかったログファイルを格納するコンテナ
 * @param [in] flag
 * @li 0: ファイルが見つからない場合はディレクトリを返す
 * @li 1: 最新のログファイルのみを返す
 * @li 2: 範囲内のログファイルすべてを返す
 * @li 3: 最新のもの二つだけ返す（月末と月初にまたがって受信した場合）
 */
static void findLogInfo(string account, string dirName, map<string, FILETIME> &logfiles, int flag = 0) {
	static unsigned long long itime = 0; // この時間よりも前のタイムスタンプのログは無視する
	if (itime == 0) {
		FILETIME ignoreTime = {0};
		SYSTEMTIME st;
		GetSystemTime(&st);
		st.wHour -= 8; // 8 時間前に更新されたログは無視していいという仕様
		SystemTimeToFileTime(&st, &ignoreTime);
		itime = ((unsigned long long)(ignoreTime.dwHighDateTime) << 32) + ignoreTime.dwLowDateTime;
	}

	const char *home = getMailDir();
	string subdir = string(home) + string(account) + string("\\") + dirName + string("\\");
	string findconst = subdir + string("*.txt");
	HANDLE dno;
	LPCSTR dir = findconst.c_str();
	WIN32_FIND_DATA fil;
	string logfile("");
	FILETIME filetime = {0};

	// ファイル検索
	if ((dno = FindFirstFile(dir, &fil)) != INVALID_HANDLE_VALUE) {
		if ((fil.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
			logfile = fil.cFileName;
			filetime = fil.ftLastWriteTime;
			while(FindNextFile(dno, &fil) != 0) {
				if ((fil.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
					if (flag == 2 || flag == 3) {
						unsigned long long ftime = ((unsigned long long)(filetime.dwHighDateTime) << 32) + filetime.dwLowDateTime;
						if (ftime >= itime) {
							logfiles.insert(pair<string, FILETIME>(subdir + logfile, filetime));
						}
						if (flag == 3) {
							if (logfiles.size() > 2) {
								logfiles.erase(logfiles.begin());
							}
						}
					}
					logfile = fil.cFileName;
					filetime = fil.ftLastWriteTime;
				}
			}
		}
		FindClose(dno);
	} else {
		logfile = "";
		filetime.dwHighDateTime = 0;
		filetime.dwLowDateTime = 0;
	}
	if (flag == 0 || logfile != "") {
		unsigned long long ftime = ((unsigned long long)(filetime.dwHighDateTime) << 32) + filetime.dwLowDateTime;
		bool ignore = flag == 2 && ftime < itime;
		if (!ignore) {
			logfiles.insert(pair<string, FILETIME>(subdir + logfile, filetime));
		}
	}
}

/**
 * ログファイル情報を取得する。
 *
 * @return ログファイル名とタイムスタンプのマップオブジェクト
 */
static map<string, FILETIME> getLogInfo() {
	const char *home = getMailDir();
	
	// サブディレクトリを取得
	int account_count;
	char **subdirs = getMailAccountDirs(home, &account_count);
	// ログファイルを取得
	map<string, FILETIME> logfiles;
	for (int i = 0; i < account_count; i++) {
		findLogInfo(subdirs[i], "受信ログ", logfiles);
	}
	if (account_count) {
		for (int i = 0; i < account_count; i++) {
			free(subdirs[i]);
		}
		free(subdirs);
	}
	return logfiles;
}


/**
 * ログファイル情報を取得する。
 *
 * @param [in] account_count アカウント数
 * @param [in] subdirs アカウント名（アカウントフォルダ名）
 * @return ログファイル名とタイムスタンプのマップオブジェクト
 */
static map<string, FILETIME> getLogInfo(int account_count, char **subdirs) {
	// ログファイルを取得
	map<string, FILETIME> logfiles;
	for (int i = 0; i < account_count; i++) {
		findLogInfo(subdirs[i], "受信ログ", logfiles);
	}

	return logfiles;
}

/**
 * メールで管理している、メール受信時に更新される可能性のあるファイルリストを返す。
 * 
 * 更新される可能性のあるファイルは、ファイル名のもっとも新しいもの２つとする。
 * 
 * @return 更新される可能性のあるファイルリスト
 */
map<string, FILETIME> getDistributedLogInfo(void) {
	const char *home = getMailDir();
	// サブディレクトリを取得
	int account_count;
	char **subdirs = getMailAccountDirs(home, &account_count);

	// ログファイルを取得
	map<string, FILETIME> logfiles;
	for (int i = 0; i < account_count; i++) {
		string findconst = string(home) + string(subdirs[i]) + "\\受信ログ\\受信ログ*.txt";

		HANDLE dno;
		LPCSTR dir = findconst.c_str();
		WIN32_FIND_DATA fil;
		char logfile[MAX_PATH] = {0};
		FILETIME filetime;

		// ファイル検索
		if ((dno = FindFirstFile(dir, &fil)) != INVALID_HANDLE_VALUE) {
			if ((fil.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
				strcpy_s(logfile, fil.cFileName);
				filetime = fil.ftLastWriteTime;
			}
			BOOL find = FALSE;
			while(FindNextFile(dno, &fil) != 0) {
				if ((fil.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
					strcpy_s(logfile, fil.cFileName);
					filetime = fil.ftLastWriteTime;
					find = true;
				}
			}
			if (!find) {
				logfile[0] = '\0';
				filetime.dwHighDateTime = 0;
				filetime.dwLowDateTime = 0;
			}
			FindClose(dno);
		} else {
			logfile[0] = '\0';
			filetime.dwHighDateTime = 0;
			filetime.dwLowDateTime = 0;
		}
		string subdir = string(home) + string(subdirs[i]) + "\\受信ログ\\" + logfile;
		logfiles.insert(pair<string, FILETIME>(subdir, filetime));
	}

	if (account_count) {
		for (int i = 0; i < account_count; i++) {
			free(subdirs[i]);
		}
		free(subdirs);
	}

	return logfiles;
}

/**
 * http://eternalwindows.jp/security/securitycontext/securitycontext05.html
 * まず、OpenProcessTokenにGetCurrentProcessの戻り値を指定して、
 * 現在のプロセスのトークンを取得します。
 * このとき、AdjustTokenPrivilegesの呼び出しが必要になる関係上、
 * TOKEN_ADJUST_PRIVILEGESを指定しています。
 * 続いて、LookupPrivilegeValueで特権名から関連するLUIDを取得し、
 * これをTOKEN_PRIVILEGES構造体に指定します。
 * 指定する特権の数は1つであるため、PrivilegeCountは1であり、
 * bEnableにはTRUEが設定されているため、Attributesには
 * SE_PRIVILEGE_ENABLEDが指定されます。
 * これにより、AdjustTokenPrivilegesの呼び出しで特権が有効になります。
 * ただし、少し注意しなければならないのは、AdjustTokenPrivilegesの戻り値が
 * 特権を実際に有効にできたかどうかを表わさないという点です。
 * この関数は、引数や構造体の指定に問題がなければTRUEを返すため、
 * 特権を有効にできたかどうかはGetLastErrorを通じて確認する必要があります。
 */
BOOL EnablePrivilege(LPTSTR lpszPrivilege, BOOL bEnable)
{
	BOOL             bResult;
	LUID             luid;
	HANDLE           hToken;
	TOKEN_PRIVILEGES tokenPrivileges;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return FALSE;
	
	if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
		CloseHandle(hToken);
		return FALSE;
	}

	tokenPrivileges.PrivilegeCount           = 1;
	tokenPrivileges.Privileges[0].Luid       = luid;
	tokenPrivileges.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;
	
	bResult = AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
	
	CloseHandle(hToken);

	return bResult && GetLastError() == ERROR_SUCCESS;
}

/**
 * ini ファイルの初期化を行う。
 */
static void init(void) {
	const char *work = getWorkDir();
	const char *home = getMailDir();
	
	// サブディレクトリを取得
	int account_count;
	char **subdirs = getMailAccountDirs(home, &account_count);

	// ログファイルを取得
	map<string, FILETIME> logfiles = getLogInfo(account_count, subdirs);

	// INIファイルオープン
	WaitForSingleObject(IniFile::hMutex, INFINITE);
	if (Debug) {
		FILE *ini;
		fopen_s(&ini, string(work).append("\\hidebiff.ini").c_str(), "r");
		FILE *backup;
		fopen_s(&backup, string(work).append("\\hidebiff.bak.ini").c_str(), "w");
		char buff[256] = "";
		while (fgets(buff, 256, ini) != NULL) {
			fprintf(backup, "%s", buff);
		}
		fclose(ini);
		fclose(backup);
	}
	IniFile ini(string(work).append("\\hidebiff.ini").c_str());

	// ToastNotify のパスを保持
	char *toastNotifyPath = ini.read("Settings", "ToastNotify");
	char *limitCount = ini.read("Settings", "LimitCount");
	char *filterLogSave = ini.read("Settings", "FilterLogSave");

	char *icon = ini.read("ToastNotify", "icon");
	char *module = ini.read("ToastNotify", "module");
//	char *bitmap = ini.read("ToastNotify", "bitmap");
//	char *textcolor = ini.read("ToastNotify", "textcolor");
	char *effect = ini.read("ToastNotify", "effect");
	char *position = ini.read("ToastNotify", "position");
	char *visibirity = ini.read("ToastNotify", "visibirity");
	char *timeout = ini.read("ToastNotify", "timeout");
	char *alarm = ini.read("ToastNotify", "alarm");

	// いったん削除して内容をクリア
	ini.del();

	// ToastNotify パスを書き出し
	ini.write("Settings", "ToastNotify", toastNotifyPath);
	ini.write("Settings", "LimitCount", limitCount);
	if (strcmp(filterLogSave, "") != 0) {
		ini.write("Settings", "FilterLogSave", filterLogSave);
	}
	free(filterLogSave);
	free(toastNotifyPath);
	free(limitCount);

	if (strlen(icon) != 0) ini.write("ToastNotify", "icon", icon);
	if (strlen(module) != 0) ini.write("ToastNotify", "module", module);
//	if (strlen(bitmap) != 0) ini.write("ToastNotify", "bitmap", bitmap);
//	if (strlen(textcolor) != 0) ini.write("ToastNotify", "textcolor", textcolor);
	if (strlen(effect) != 0) ini.write("ToastNotify", "effect", effect);
	if (strlen(position) != 0) ini.write("ToastNotify", "position", position);
	if (strlen(visibirity) != 0) ini.write("ToastNotify", "visibirity", visibirity);
	if (strlen(timeout) != 0) ini.write("ToastNotify", "timeout", timeout);
	if (strlen(alarm) != 0) ini.write("ToastNotify", "alarm", alarm);

	free(icon);
	free(module);
//	free(bitmap);
//	free(textcolor);
	free(effect);
	free(position);
	free(visibirity);
	free(timeout);
	free(alarm);

	// メールディレクトリを出力
	ini.write("Settings", "MailDir", home);

	// 受信ログをすべて出力
	map<string, FILETIME>::iterator it = logfiles.begin();
	for (int i = 0; it != logfiles.end(); i++, it++) {
		char buf[MAX_PATH + 1];

		strcpy_s(buf, it->first.c_str());
		int end = (int)(strstr(buf, "\\受信ログ\\") - buf);
		int start = 0;
		for (int k = 1; buf[end - k] != '\\'; k++) {
			start = end - k;
		}
		int j;
		for (j = 0; j < account_count; j++) {
			if (strncmp(buf + start, subdirs[j], end - start) == 0) {
				break;
			} else if (j == account_count - 1) {
				j = -1;
				break;
			}
		}
		if (j == -1) {
			// TODO: アカウント情報が変更されたりなんだりして、見つからない
			msgbox("ERROR 1: 見つからない");
			exit(0);
		}

		// アカウント情報を出力
		_itoa_s(i, buf, 10);
		ini.write("Accounts", string("Account").append(buf).c_str(), subdirs[j]);

		ini.write(subdirs[j], "LogFile", it->first.c_str());

		FILETIME time = it->second;
		if (time.dwLowDateTime != 0 && time.dwHighDateTime != 0) {
			_ltoa_s(time.dwHighDateTime, buf + 16, sizeof(buf) - 16, 16);
			_ltoa_s(time.dwLowDateTime, buf + 16 + strlen(buf + 16), sizeof(buf) - 16 - strlen(buf + 16), 16);
			size_t zeroAddCount = 16 - strlen(buf + 16);
			for (size_t i = 0; i < zeroAddCount; i++) {
				buf[16 - i - 1] = '0';
			}
			ini.write(subdirs[j], "LogTime", buf + 16 - zeroAddCount);
		} else {
			strcpy_s(buf, "0");
			ini.write(subdirs[j], "LogTime", buf);
		}

		// 最新の受信ログのメッセージ数をカウントしておかなければならない
		if (time.dwHighDateTime != 0 && time.dwLowDateTime != 0) {
			char delim[6] = DELIM;
			int count = 0;

			char *contents;
			char *cp = contents = getFileContents(it->first.c_str(), NULL);
			while (contents) {
				contents = strstr(contents, delim);
				if (contents) {
					count++;
					contents += 5;
				}
			}
			if (cp) {
				free(cp);
			}
			_itoa_s(count, buf, 10);
			ini.write(subdirs[j], "LogCount", buf);
		} else {
			ini.write(subdirs[j], "LogCount", "0");
		}
	}

	if (account_count) {
		for (int i = 0; i < account_count; i++) {
			free(subdirs[i]);
		}
		free(subdirs);
	}

	int linenum = 0;
	if (PathFileExists(string(home).append("filterlog.txt").c_str())) {
		char *filter = getFileContents(string(home).append("filterlog.txt").c_str(), NULL);
		if (filter) {
			char *cp = filter;
			while ((cp = strstr(cp, "\r\n")) != NULL) {
				linenum++;
				cp += 2;
			}
			linenum--; // 最後の一行は空白行のためカウントしない
			free(filter);
		}
	}
	ini.write("Settings", "FilterLog", linenum);
	ReleaseMutex(IniFile::hMutex);
}

/**
 * 各アカウントのフィルター情報。
 * 
 * map の first はアカウント名。
 * map の second には振り分けられる可能性のあるフォルダを一覧で持っている。
 * この一覧は vector だが、内容は重複せず、出現順を保持している。
 * 
 * vector のフォルダはたとえば以下のような形式で保持している。
 * 
 * 受信\foo\bar
 */
static map<string, vector<string>> filters;

/**
 * 各アカウントの "filter.txt" を読み込んだかどうか。
 */
static map<string, bool> isReadFilter;

/**
 * "filterlog.txt" を読み込んだかどうかをあらわすフィールド。
 */
static bool isReadFilterLog = false;

/**
 * 通知を行わないメールの Message-Id の列挙。
 */
static vector<string> ignoreMessageId;

/**
 * 受信した合計メール数。
 */
static int totalReceivedMail = 0;

/**
 * メールを表すクラス。
 */
class Mail {
#define splitToken(base)	(temp = strstr(base, "\t"), temp[0] = '\0', ++temp) ///< タブ文字区切りを分割するマクロ。必ずタブ文字が含まれるという前提。
public:
	/**
	 * コンストラクタ。
	 *
	 * @param [in] account メールアカウント名
	 * @param [in] contents ヘッダも含めたメール本文
	 */
	Mail(const char *account, const char *contents) : account(account), initialized(false), ignoreNotify(false), codepage(CP_ACP) {
		char buf[20] = {'\0'};
		strncpy_s(buf, contents, _TRUNCATE);
		key = string(buf);
		key[key.find(' ')] = '\0';

		if (!isReadFilterLog) {
			isReadFilterLog = true;
			addFilter(account, "受信"); // 振り分けにヒットしなかったら受信箱

			if (PathFileExists(string(getMailDir()).append("filterlog.txt").c_str())) {
				char *filter = getFileContents(string(getMailDir()).append("filterlog.txt").c_str(), NULL);
				if (filter) {
					WaitForSingleObject(IniFile::hMutex, INFINITE);
					IniFile ini((string(getWorkDir()) + "\\hidebiff.ini").c_str());
					char *numstr = ini.read("Settings", "FilterLog");
					ReleaseMutex(IniFile::hMutex);
					int filnum = atoi(numstr);
					free(numstr);
					int linenum = 0;
					char *cp = filter;
					char *lineend;
					while ((lineend = strstr(cp, "\r\n")) != NULL) {
						if (linenum >= filnum) {
							if (strcmp(cp, "\r\n") == 0) {
								break;
							}
							size_t size = lineend - cp + 1;
							char *line = (char *)malloc(size);
							strncpy_s(line, size, cp, lineend - cp);
							line[lineend - cp] = '\0';

							char *temp;

							char *tokenDate = line;											// 日付
							char *tokenAccount = splitToken(tokenDate);						// アカウント
							char *tokenSendOrRecv = splitToken(tokenAccount);				// 送/受
							char *token3 = splitToken(tokenSendOrRecv);						// 自
							char *tokenFromOrTo = splitToken(token3);						// From:/To:
							char *tokenMessageId = splitToken(tokenFromOrTo);				// Message-Id
							char *tokenFuriwakeJyoken;										// 振り分けヒット条件
							if (strstr(tokenMessageId, "Message-Id:") != tokenMessageId) { // Message-Id が存在しないケース
								tokenFuriwakeJyoken = tokenMessageId;
								tokenMessageId = NULL;
							} else {
								tokenFuriwakeJyoken = splitToken(tokenMessageId);
							}
							char *tokenFuriwakeSaki = NULL;									// 振り分け先
							if (strstr(tokenFuriwakeJyoken, "\t")) {
								tokenFuriwakeSaki = splitToken(tokenFuriwakeJyoken);
							}
							char *tokenOtherInfo = NULL;									// その他情報
							if (tokenFuriwakeSaki != NULL && strstr(tokenFuriwakeSaki, "\t")) {
								tokenOtherInfo = splitToken(tokenFuriwakeSaki);
							}

							bool skip = false;
							if (strncmp(tokenSendOrRecv + 1, "送", 2) == 0) {
								skip = true;
							}
							if (!skip) {
								if (tokenOtherInfo) {
									if (strcmp(tokenOtherInfo, "通知無し") == 0) {
										// FIXME: 通知なしでかつ Message-Id のないメールは通知してしまう
										if (tokenMessageId) {
											tokenMessageId += strlen("Message-Id:");
											ignoreMessageId.push_back(string(tokenMessageId));
											skip = true;
										}
									}
								}
							}
							if (!skip) {
								if (strcmp(tokenFuriwakeJyoken, "振り分け動作なし") == 0) {
									if (strcmp(account, tokenAccount) != 0) {
										addFilter(string(tokenAccount), "受信");
									}
									skip = true;
								}
							}

							if (!skip) {
								if (strstr(tokenFuriwakeSaki, "移動:アカウントのバイパス:") == tokenFuriwakeSaki) {
									// 詳細ログなら無視してよい
								} else {
									if (strstr(tokenFuriwakeSaki, "移動:") == tokenFuriwakeSaki) {
										string dist;
										dist = tokenFuriwakeSaki + strlen("移動:");
										addFilter(string(tokenAccount), dist);
									}
								}
							}
							free(line);
						}
						cp = lineend + 2;
						linenum++;
					}
					free(filter);
				}
			}
		}
	}

	/**
	 * デストラクタ。
	 * 特に何もしない。
	 */
	~Mail() {
	}

	/**
	 * 引数で与えられた #text のダブルコーテーションを '\' でエスケープする。
	 *
	 * @param [in] text 対象の文字列
	 * @return エスケープ後の文字列
	 */
	wstring escapeDQuote(wstring text) {
		size_t pos;
		if ((pos = text.find('"')) != string::npos) {
			wstring s = text;
			while (pos != string::npos) {
				s.replace(pos, 1, L"\\\"", 2);
				pos = s.find('"', pos + 2);
			}
			return s;
		} else {
			return text;
		}
	}

	/**
	 * ヘッダを取得する。
	 *
	 * @param [in] headerName ヘッダ名
	 * @return ヘッダ
	 */
	wstring getHeader(wstring &headerName) {
		return headers[headerName];
	}

	/**
	 * ヘッダを取得する。
	 *
	 * @param [in] headerName ヘッダ名
	 * @return ヘッダ
	 */
	wstring getHeader(const wchar_t *headerName) {
		wstring name(headerName);
		return getHeader(name);
	}

	/**
	 * メールの本文を設定する。
	 *
	 * @param [in] aBody メールの本文
	 */
	void setBody(string aBody) {
		const char *bodyBytes = aBody.c_str();
		DWORD flag = MB_ERR_INVALID_CHARS;
		int size = MultiByteToWideChar(codepage, flag, bodyBytes, -1, NULL, 0);
		if (size == 0) {
			flag = MB_PRECOMPOSED; // GB2312 に日本語が含まれている場合などは MB_ERR_INVALID_CHARS ではエラーになるためやり直してみる
			size = MultiByteToWideChar(codepage, flag, bodyBytes, -1, NULL, 0);
		}
		wchar_t *wcs = (wchar_t *)malloc(sizeof(wchar_t) * size);
		MultiByteToWideChar(codepage, flag, bodyBytes, -1, wcs, size);

		body = wstring(wcs);
	}

	/**
	 * メールの本文を返す。
	 *
	 * @return メールの本文
	 */
	wstring & getBody() {
		return body;
	}

	/**
	 * メールのキーを返す。
	 *
	 * @return メールのキー
	 */
	string & getKey() {
		return key;
	}

	/**
	 * メールのファイルパスを返す。
	 *
	 * @return メールのファイルパス
	 */
	string & getFilePath() {
		return filepath;
	}

	/**
	 * メールのインデックスを設定する。
	 *
	 * @param [in] メールインデックス
	 */
	void setIndex(int aIndex) {
		index = aIndex;
	}

	/**
	 * メールのファイルパスを設定する。
	 *
	 * @param [in] メールのファイルパス
	 */
	void setFilePath(string aFilePath) {
		filepath = aFilePath;
	}

	/**
	 * メールのインデックスを取得する。
	 *
	 * @return メールのインデックス
	 * @note このメソッドが返すバッファは開放しなくてもよい代わりにマルチスレッドに対応していない。
	 */
	const char * getIndex() {
		static char buff[10];
		memset(buff, 0, sizeof(buff));
		_itoa_s(index, buff, 10);
		return buff;
	}

	/**
	 * メールが初期化されたかどうかを返す。
	 *
	 * @return メールが初期化済みなら true、そうでないなら false
	 */
	bool isInitialized() {
		return initialized;
	}
	
	/**
	 * ヘッダをパースしてヘッダエントリを初期化する。
	 *
	 * 引数として渡される #header は、ヘッダに含まれる "X-Body-Content-Type"
	 * の文字コードセットでデコードされているため、そのまま使用すると文字化けする可能性がある。
	 * これを防ぐためのに、このメソッドはいったん Unicode に変換してからヘッダエントリに追加する。
	 * 
	 * @param [in] header デコード済みのヘッダ
	 */
	void parseHeader(string &header) {
		// ヘッダの余計な改行をすべて除去
		size_t pos = 0;
		while ((pos = header.find("\r\n\t", pos)) != string::npos) {
			header.replace(pos, 3, "");
		}
		pos = 0;
		while ((pos = header.find("\r\n ", pos)) != string::npos) {
			header.replace(pos, 3, "");
		}

		initCodePage(header);
		const char *headerBytes = header.c_str();
		DWORD flag = MB_ERR_INVALID_CHARS;
		int size = MultiByteToWideChar(codepage, flag, headerBytes, -1, NULL, 0); // MB_PRECOMPOSED だと UTF-8 でエラーになったので、しばらく MB_ERR_INVALID_CHARS で試してみる
		if (size == 0) {
			flag = MB_PRECOMPOSED;
			size = MultiByteToWideChar(codepage, flag, headerBytes, -1, NULL, 0);
		}
		wchar_t *utf16Header = (wchar_t *)malloc(sizeof(wchar_t) * size);
		MultiByteToWideChar(codepage, flag, headerBytes, -1, utf16Header, size);

		boost::basic_regex<wchar_t> r(L"^([^:]+): ([^\r\n]+)\r\n");
		boost::match_results<const wchar_t *> m;

		while (boost::regex_search(utf16Header, m, r)) {
			wstring h = m.str(1);
			std::transform(h.begin(), h.end(), h.begin(), std::tolower);
			wstring v = m.str(2);
			wstring l = m.str();
			headers.insert(pair<wstring, wstring>(h, v));
			utf16Header += l.size();
		}

		for (vector<string>::iterator it = ignoreMessageId.begin(); it != ignoreMessageId.end(); ++it) {
			wstring msgid = getHeader(L"message-id");
			size_t length = msgid.size();
			for (size_t i = 0; i < length; ++i) {
				if (msgid[i] != (*it)[i]) {
					break;
				} else if (length - 1 == i) {
					ignoreNotify = true;
					ignoreMessageId.erase(it);
					break;
				}
			}
		}
		initialized = true;
	}

	/**
	 * 通知を無視するメールかどうかを返す。
	 *
	 * @return 無視するメールなら true、そうでないなら false
	 */
	bool isIgnoreNotify() {
		return ignoreNotify;
	}

	/**
	 * メールのコードページを初期化する。
	 *
	 * メールヘッダから "X-Body-Content-Type:" を探し出し、
	 * そこに含まれている charset を取り出し、その名前からコードページを初期化する。
	 *
	 * @param [in] header デコード済みのヘッダ
	 */
	void initCodePage(string &header) {
		size_t off;
		if ((off = header.find("X-Body-Content-Type: ")) != string::npos ||
				(off = header.find("Content-Type: ")) != string::npos ||
				(off = header.find("Content-type: ")) != string::npos ||
				(off = header.find("content-type: ")) != string::npos ||
				(off = header.find("content-Type: ")) != string::npos) {
			string charset = header.substr(off);
			charset = charset.substr(0, charset.find("\r\n"));
			charset = charset.substr(charset.find("charset=") + strlen("charset="));
			charset = charset.substr(0, charset.find(";"));
			charset = charset.substr(0, charset.find(" "));
			if (charset[0] == '"') {
				charset = charset.substr(1, charset.size() - 2);
			}
			// 文字コードセットを元にコードページを割り出す
			if (charset == "gb2312" || charset == "GB2312") {
				// 中国語簡体字
				codepage = 936;
			} else if (charset == "EUC-KR" || charset == "euc-kr") {
				// 韓国語
				codepage = 949;
			} else if (charset == "utf-8" || charset == "UTF-8") {
				// UTF-8
				codepage = CP_UTF8;
			} else {
				// デフォルトは日本語コードページ
				codepage = 932;
			}
		}
	}

private:
	string account;					///< アカウント名。
	string key;						///< メールのキー。
	map<wstring, wstring> headers;	///< ヘッダの一覧。
	wstring body;					///< メールの本文。
	string filepath;				///< メールのファイルパス。
	int index;						///< メールのインデックス。
	bool initialized;				///< 初期化済みフラグ。
	bool ignoreNotify;				///< 無視フラグ。
	unsigned int codepage;			///< メールのコードページ。

	/**
	 * フィルタ（#filters）に振り分け先候補を追加する。
	 *
	 * @param [in] tempAccount 振り分け先アカウント
	 * @param [in] dist 振り分け先フォルダ
	 */
	void addFilter(string tempAccount, const char *dist) {
		addFilter(tempAccount, string(dist));
	}

	/**
	 * フィルタ（#filters）に振り分け先候補を追加する。
	 *
	 * @param [in] tempAccount 振り分け先アカウント
	 * @param [in] dist 振り分け先フォルダ
	 */
	void addFilter(string tempAccount, string dist) {
		bool duplicate = false;
		for (size_t i = 0; i < filters[tempAccount].size(); i++) {
			if (filters[tempAccount][i].compare(dist) == 0) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			filters[tempAccount].push_back(dist);
		}
	}

};

static vector<Mail> mails;			///< 受信したメールを保持するベクター。
static HANDLE hMailsMutex = NULL;	///< #mails を操作する際の排他オブジェクト。

/**
 * メールを初期化する関数。
 *
 * 振り分けられている可能性のある場所を #filters から抽出し、
 * 実際に振り分けられた場所を特定した後、#Mail クラスのオブジェクトを作成、
 * #mails にオブジェクトを保持する。
 */
static void initializeMail(void) {
	for (map<string, vector<string> >::iterator fit = filters.begin(); fit != filters.end(); fit++) {
		// ログを開いてメールを探す
		for (size_t i = 0; i < fit->second.size(); i++) {
			map<string, FILETIME> logfiles;
			findLogInfo(fit->first, fit->second[i], logfiles, 3);
			
			debug("<探したファイル>\n");
			for (map<string, FILETIME>::iterator it = logfiles.begin(); it != logfiles.end(); it++) {
				debug(it->first + "\n");
				DWORD size;
				char *text = NULL;
				for (int i = 0; i < 5; i++) { // 秀丸メール本体と競合して読めないケースがあるのでリトライする
					text = getFileContents(it->first.c_str(), &size);
					if (text) {
						break;
					} else {
						Sleep(500);
					}
				}

				WaitForSingleObject(hMailsMutex, INFINITE);
				for (vector<Mail>::iterator mit = mails.begin(); mit != mails.end(); mit++) {
					if (mit->isInitialized()) {
						continue;
					}
					char *pos = strstr(text, mit->getKey().c_str());
					if (pos) {
						mit->setIndex((int)(pos - text - 15)); // -15 はユニークキーの前に付与されている文字数
						mit->setFilePath(it->first);

						//////////////////////
						// メールを読み出す //
						//////////////////////
						char delim[6] = DELIM2;
						char *mailend = strstr(pos, delim);
						if (mailend) {
							mailend[0] = '\0';
						}

						string s = string(pos);
						string header = s.substr(s.find("\r\n") + 2);
						size_t bodyIndex = header.find("\r\n\r\n") + 2;
						header = header.substr(0, bodyIndex);
						bodyIndex = s.find("\r\n\r\n") + 4;
						string body = s.substr(bodyIndex);
						mit->parseHeader(header);
						// FIXME: body の行数は変更可能に
						{
							size_t crln = 0;
							for (int j = 0; j < 6; j++) {
								size_t temp = body.find("\r\n", crln);
								if (temp != string::npos) {
									crln = temp + 2;
								} else {
									crln = body.size();
									break;
								}
							}
							if (crln > 300) { // 時々改行コードが \r\n ではないメールがあったりする（JIRA とか）
								crln = 300;
							}
							body = body.substr(0, crln);

							while (body.size() > 2 && body[body.size() -2] == '\r') {
								body = body.substr(0, body.size() - 2);
							}
						}
						mit->setBody(body);
						if (mailend) {
							mailend[0] = delim[0];
						}
					}
				}
				ReleaseMutex(hMailsMutex);
				free(text);
			}
			debug("</探したファイル>\n");
		}
	}
}

/**
 * メール受信後の処理を行なう。
 *
 * 通常であれば、受信したメール数はすべてのアカウントの受信ログの追加分と等しいが
 * そうならない場合もままあるように思える。
 *
 * @li INI ファイルが削除、改ざんされているかもしれない点を考慮する。
 * @li メールディレクトリの設定が変更されているかもしれない点を考慮する。
 * @li メールアカウントが追加、変更、削除されているかもしれない点を考慮する。
 * @li 受信ログが削除、改ざんされているかもしれない点を考慮する。 ← これは無理
 *
 * @param [in] count 受信メール総数
 */
static void received(int count) {
	const char *work = getWorkDir();
	const char *home = getMailDir();
	char *inivalue = NULL;
	totalReceivedMail += count;
	debug("<received>\n");
	debug("received:totalReceivedMail[");
	debug(totalReceivedMail);
	debug("] count[");
	debug(count);
	debug("]\n");

	// まずは INI ファイルに異変がないか確認する
	WaitForSingleObject(IniFile::hMutex, INFINITE);
	IniFile ini(string(work).append("\\hidebiff.ini").c_str());

	// メールアカウントが追加、変更、削除されているかもしれない点を確認
	int num;
	char **accounts = getMailAccountDirs(home, &num);

	// ログファイルが更新されているかどうかをチェック
	unsigned int totalcount = 0; totalcount;
	map<string, FILETIME> logfiles = getLogInfo();
	char buf[MAX_PATH + 1];
	for (int j = 0; j < num; j++) {
		inivalue = ini.read(accounts[j], "LogTime", NULL);
		FILETIME initime;
		memset(buf, 0, sizeof(buf));

		strncpy_s(buf, inivalue, 16);
		if (buf[0] == '0' && buf[1] == '\0') {
			initime.dwHighDateTime = 0;
			initime.dwLowDateTime = 0;
		} else {
			initime.dwHighDateTime = hexatoint64(buf);
			strcpy_s(buf, inivalue + 8);
			initime.dwLowDateTime = hexatoint64(buf);
		}
		free(inivalue);
		inivalue = NULL;
		int update = 0; //< 1: 元のファイルに追加受信  2: 別のファイルに受信
		inivalue = ini.read(accounts[j], "LogFile", NULL);
		for (size_t n = strlen(inivalue); inivalue[n] != '\\'; n--) {
			inivalue[n] = '\0';
		}
		map<string, FILETIME>::iterator it;
		for (it = logfiles.begin(); it != logfiles.end(); it++) {
			strcpy_s(buf, it->first.c_str());
			char *pos = strstr(buf, "\\受信ログ\\");
			pos[10] = '\0';
			if (strcmp(buf, inivalue) == 0) {
				break;
			}
		}
		free(inivalue);
		inivalue = NULL;
		if (it != logfiles.end()) {
			inivalue = ini.read(accounts[j], "LogFile", NULL);
			if (strcmp(inivalue, it->first.c_str()) == 0) { // ログファイルは同じ
				if (initime.dwHighDateTime != it->second.dwHighDateTime ||
						initime.dwLowDateTime != it->second.dwLowDateTime) { // ログ時間は違う
					update = 1; // このアカウントは元のログファイルにメール受信している
				}
			} else {
				update = 2; // このアカウントは別のログファイルにメール受信している
				if (initime.dwHighDateTime != 0 || initime.dwLowDateTime != 0) {
					HANDLE dno;
					LPCSTR dir = inivalue;
					WIN32_FIND_DATA fil;
					FILETIME filetime;
					filetime.dwHighDateTime = 0;
					filetime.dwLowDateTime = 0;

					// ファイル検索
					if ((dno = FindFirstFile(dir, &fil)) != INVALID_HANDLE_VALUE) {
						if ((fil.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
							filetime = fil.ftLastWriteTime;
						}
					}
					FindClose(dno);
					if (initime.dwHighDateTime != filetime.dwHighDateTime ||
							initime.dwLowDateTime != filetime.dwLowDateTime) {
						update += 1; // このアカウントは元の最初のファイルも更新されている
					}
				}
			}
			strcpy_s(buf, inivalue);
			free(inivalue);
			inivalue = NULL;

			if (update == 0) {
				continue;
			}

			inivalue = ini.read(accounts[j], "LogCount", NULL);
			int inicount = atoi(inivalue);
			free(inivalue);
			inivalue = NULL;



			if (update == 2) {
				// 別のログファイルにのみメールを受信している場合
logfile_another_case:
				// ファイル検索
				HANDLE dno;
				char dir[MAX_PATH + 1];
				WIN32_FIND_DATA fil;
				char nextFileName[MAX_PATH + 1] = {0};
				char *currentFileName;

				strcpy_s(dir, buf);
				for (;;) {
					size_t len = strlen(dir);
					if (dir[len - 1] == '\\') {
						break;
					} else {
						dir[len - 1] = '\0';
					}
				}
				strcat_s(dir, "受信ログ*.txt");

				currentFileName = buf + strlen(buf);
				while (currentFileName[0] != '\\') {
					currentFileName--;
				}
				currentFileName++;

				// FAT ならファイル作成順に、NTFS ならファイル名順に取得できるので、以下のロジックで問題ない
				if ((dno = FindFirstFile(dir, &fil)) != INVALID_HANDLE_VALUE) {
					if (initime.dwHighDateTime == 0 && initime.dwLowDateTime == 0) {
						strcpy_s(nextFileName, fil.cFileName);
					} else {
						while (FindNextFile(dno, &fil)) {
							if (strcmp(currentFileName, fil.cFileName) < 0) {
								strcpy_s(nextFileName, fil.cFileName);
								break;
							}
						}
					}
				}
				FindClose(dno);

				if (nextFileName[0]) {
					strcpy_s(currentFileName, sizeof(buf) - (currentFileName - buf), nextFileName);
					inicount = 0;
					goto logfile_update_case;
				}
			} else if (update == 1) {
logfile_update_case:
				// 元のログファイルに追加受信している場合
				char *contents = getFileContents(buf, NULL);
				char *cp = contents;
				char delim[6] = DELIM;
				int cc = 0;

				while (contents) {
					contents = strstr(contents, delim);
					if (contents) {
						cc++;
						contents += 7;
						if (cc > inicount) {
							// これ以降は新規に受信したメール
							Mail mail(accounts[j], contents);
							WaitForSingleObject(hMailsMutex, INFINITE);
							mails.push_back(mail);
							ReleaseMutex(hMailsMutex);
						}
					}
				}
				if (cp) {
					free(cp);
				}
				if (update == 3) {
					goto logfile_another_case;
				}
			} else if (update == 3) {
				goto logfile_update_case;
			}
		}
	}
	ReleaseMutex(IniFile::hMutex);
	initializeMail();
	
	if (num) {
		for (int i = 0; i < num; i++) {
			free(accounts[i]);
		}
		free(accounts);
	}
	if (inivalue) {
		free(inivalue);
	}
	debug("</received>\n");
}

/**
 * ウィンドウを画面中央に設定する。
 *
 * @param [in] hWnd 対象のウィンドウハンドル
 */
static void SetWinCenter(HWND hWnd) {
    HWND hDeskWnd;
    RECT deskrc, rc;
    int x, y;
    
    hDeskWnd = GetDesktopWindow();
    GetWindowRect(hDeskWnd, (LPRECT)&deskrc);
    GetWindowRect(hWnd, (LPRECT)&rc);
    x = (deskrc.right - (rc.right-rc.left)) / 2;
    y = (deskrc.bottom - (rc.bottom-rc.top)) / 2;
    SetWindowPos(hWnd, HWND_TOP, x, y, (rc.right-rc.left), (rc.bottom-rc.top),SWP_SHOWWINDOW); 
}

/**
 * ToastNotify.exe を起動し、メール着信を通知する。
 */
static void toastNotice(void) {
	int currentIndex = 1;
	const char *work = getWorkDir();
	char *inivalue;
	wchar_t buff[MAX_PATH * 2];

	// まずは INI ファイルに異変がないか確認する
	WaitForSingleObject(IniFile::hMutex, INFINITE);
	IniFile ini(string(work).append("\\hidebiff.ini").c_str());

	// ToastNotify のパスを取得する
	inivalue = ini.read("Settings", "ToastNotify");
	ReleaseMutex(IniFile::hMutex);
	size_t num;
	mbstowcs_s(&num, buff, inivalue, sizeof(buff));
	wstring toastNotify = buff;
	free(inivalue);

	WaitForSingleObject(hMailsMutex, INFINITE);
	ReleaseMutex(hMailsMutex);
	debug("<toastNotice>\n");
	while (!mails.empty()) {
		WaitForSingleObject(hMailsMutex, INFINITE);
		debug("totalReceiveMail:");
		debug(totalReceivedMail);
		debug("\n");
		debug("mails.size():");
		debug((int)mails.size());
		debug("\n");
		for (size_t i = mails.size(); i > 0; i--) {
			debug("mails[");
			debug(i - 1);
			debug("][");
			debug(mails[i - 1].getHeader(L"subject"));
			debug("]\n");
		}
		Mail mail = mails[0];
		mails.erase(mails.begin());
		ReleaseMutex(hMailsMutex);
		if (mail.isIgnoreNotify() || !mail.isInitialized()) {
			if (!mail.isInitialized()) {
// FIXME: 初期化ミスをポップアップしても、ユーザには邪魔なだけのような気がする
//				MessageBox(NULL, "初期化ミス", "メールが初期化されていないので無視されました。", MB_OK);
				debug("<初期化ミス key=\"");
				debug(mail.getKey());
				debug("\"/>\n");
			}
			debug("<無視したメール>\n");
			debug(mail.getKey());
			debug("\n");
			debug("Subject: ");
			debug(mail.getHeader(L"subject"));
			debug("\n");
			debug("</無視したメール>\n");

			WaitForSingleObject(hMailsMutex, INFINITE);
			ReleaseMutex(hMailsMutex);
			continue;
		}
		wstring title;
		
		char temp[6];

		title += L"[";
		_itoa_s(currentIndex++, temp, 10);
		mbstowcs_s(&num, buff, temp, sizeof(buff));
		title += buff;
		title += L"/";
		_itoa_s(totalReceivedMail, temp, 10);
		mbstowcs_s(&num, buff, temp, sizeof(buff));
		title += buff;
		title += L"] " + mail.escapeDQuote(mail.getHeader(L"subject")) + L"\n" + mail.escapeDQuote(mail.getHeader(L"from"));

		// オプション
		string iconDetault = string(work) + "\\hidebiff.exe,0";
		string moduleDefault = string(work) + "\\hidebiff.tnm";
		string timeoutDefault = "5";

		char *icon = ini.read("ToastNotify", "icon", iconDetault.c_str());
		char *module = ini.read("ToastNotify", "module", moduleDefault.c_str());
//		char *bitmap = ini.read("ToastNotify", "bitmap");
//		char *textcolor = ini.read("ToastNotify", "textcolor");
		char *effect = ini.read("ToastNotify", "effect");
		char *position = ini.read("ToastNotify", "position");
		char *visibirity = ini.read("ToastNotify", "visibirity");
		char *timeout = ini.read("ToastNotify", "timeout", timeoutDefault.c_str());
		char *alarm = ini.read("ToastNotify", "alarm");

		wstring cmd = L"\"" + toastNotify + L"\"";
		mbstowcs_s(&num, buff, icon, sizeof(buff));
		cmd += L" /i \"" + wstring(buff) + L"\"";
		mbstowcs_s(&num, buff, module, sizeof(buff));
		cmd += L" /m \"" + wstring(buff) + L"\"";
		mbstowcs_s(&num, buff, timeout, sizeof(buff));
		cmd += L" /o " + wstring(buff);
//		cmd += L" /o 0";  // 無限待ち（デバッグ用）
		cmd += L" /w";
		mbstowcs_s(&num, buff, work, sizeof(buff));
		cmd += L" /p \"" + wstring(buff) + L"\\hidebiff.exe\"";
		cmd += L" /pa \"-s ";
		MultiByteToWideChar(CP_ACP, 0, mail.getFilePath().c_str(), -1, buff, sizeof(buff) / sizeof(wchar_t));
		cmd += buff;
		cmd += L"/";
		mbstowcs_s(&num, buff, mail.getIndex(), sizeof(buff));
		cmd += buff;
		cmd += L"\"";
// 以下は現在不採用のオプション
//		if (strlen(bitmap) != 0) cmd += " /b \"" + string(bitmap) + "\"";
//		if (strlen(textcolor) != 0) cmd += " /tc " + string(textcolor);
		if (strlen(effect) != 0) {
			mbstowcs_s(&num, buff, effect, sizeof(buff));
			cmd += L" /e " + wstring(buff);
		}
		if (strlen(position) != 0) {
			mbstowcs_s(&num, buff, position, sizeof(buff));
			cmd += L" /ps " + wstring(buff);
		}
		if (strlen(visibirity) != 0) {
			mbstowcs_s(&num, buff, visibirity, sizeof(buff));
			cmd += L" /v " + wstring(buff);
		}
		if (strlen(alarm) != 0) {
			mbstowcs_s(&num, buff, alarm, sizeof(buff));
			cmd += L" /a \"" + wstring(buff) + L"\"";
		}

		cmd += L" /t \"" + title + L"\"";
		cmd += L" /c \"" + mail.escapeDQuote(mail.getBody()) + L"\"";


		free(icon);
		free(module);
//		free(bitmap);
//		free(textcolor);
		free(effect);
		free(position);
		free(visibirity);
		free(timeout);
		free(alarm);

		debug("<ToastNotify>\n");
		debug(cmd);
		debug("\n</ToastNotify>\n");


		PROCESS_INFORMATION pi;
		STARTUPINFOW si;
		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		CreateProcessW(NULL, (LPWSTR)cmd.c_str(), NULL, NULL, FALSE, HIGH_PRIORITY_CLASS, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);

		WaitForSingleObject(hMailsMutex, INFINITE);
		ReleaseMutex(hMailsMutex);
	}
	debug("</toastNotice>\n");
}

/**
 * バージョン情報ダイアログのメッセージハンドラ。
 *
 * @param [in] hDlg ダイアログハンドル
 * @param [in] message メッセージ識別子
 * @param [in] wParam メッセージ識別子によって意味が異なるパラメータ
 * @return 処理を行った場合は TRUE、そうでない場合は FALSE
 */
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM) {
	switch (message) {
	case WM_INITDIALOG:
		SetProp(hDlg, "hidebiff.exe:prop", (HANDLE)1);
		SetWinCenter(hDlg);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;

	case WM_DESTROY:
		RemoveProp(hDlg, "hidebiff.exe:prop");
		break;
	}
	return FALSE;
}

/**
 * メイン処理。
 *
 * コマンドライン引数の解析やその後の処理、
 * 多重起動時の後発プロセスからの処理委譲などがこの関数に対して行われる。
 *
 * ※ 名前が紛らわしいが、内部でループしているわけではない。
 *
 * @param [in] lpCmdLine コマンドライン引数
 */
static void mainLoop(LPTSTR lpCmdLine) {
	if (strstr(lpCmdLine, "-i") == lpCmdLine) {
		init();
	} else if (strstr(lpCmdLine, "-r") == lpCmdLine) {
		char countstr[6] = {0};
		for (int i = 0; i < sizeof countstr; i++) {
			char c;
			if ((c = lpCmdLine[i + 3]) != ' ') {
				countstr[i] = c;
			} else {
				break;
			}
		}
		int count = atoi(countstr);
		received(count);
	} else if (strstr(lpCmdLine, "-s") == lpCmdLine) {
		char *turukameDir = (char *)get_reg_value(HIDEMARU_MAIL_DIR_KEY, NULL);
		string mailLaunchCmd = turukameDir;
		mailLaunchCmd += "TuruKame.exe";
		free(turukameDir);

		size_t len = strlen(lpCmdLine);
		char *tokens = (char *)malloc(len);
		strcpy_s(tokens, len, lpCmdLine + 3);
		char *cp = tokens;
		(*strstr(cp, "/")) = '\0';
		string filePath = cp;
		cp += strlen(cp) + 1;
		string index = cp;
		free(tokens);

		mailLaunchCmd = "\"" + mailLaunchCmd + "\"";
		mailLaunchCmd += " /vf \"" + filePath + "\" " + index;

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		CreateProcess(NULL, (LPSTR)mailLaunchCmd.c_str(), NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		// 秀丸メールを最前面に移動する
		HWND hWnd = FindWindowA("TuruKameFrame", NULL);
		if (hWnd) {
			if (IsIconic(hWnd)) {
				ShowWindow(hWnd, SW_RESTORE);
			}
			if ((GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != WS_EX_TOPMOST) {
				SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_NOMOVE | SWP_NOSIZE);
			}
		}
	} else if (strstr(lpCmdLine, "-d") == lpCmdLine) {
		DeleteFile(string(getMailDir()).append("filterlog.txt").c_str());
	}
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_ENDSESSION:
		if (wp == 1) { // セッションが終了しようとしている
			ExitProcess(0);
		}
		break;

	default:
		return DefWindowProc(hWnd, msg, wp, lp);
	}
	return 0;
}

static HWND createWindow() {
	HWND hWnd = NULL;
	LPTSTR className = TEXT("hidebiff");
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof WNDCLASSEX);
	wc.cbSize = sizeof WNDCLASSEX;
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInst;
	wc.hIcon = LoadIcon(hInst, (LPCTSTR)IDI_ICON1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = className;
	wc.hIconSm = wc.hIcon;
	if (RegisterClassEx(&wc)) {
		hWnd = CreateWindow(className, className, 0,
			0, 0, 0, 0, NULL, NULL, hInst, NULL);
	}
	return hWnd;
}


/**
 * メインエントリポイント。
 * 
 * @param [in] lpCmdLine この引数には先頭にオプションが付与される。
 * オプションの意味は以下の通り
 * @li -r: 秀丸メールがメールを受信した場合にこのオプションで呼び出される。後に続く数字は受信したメールの数。
 * @li -c: -r で呼び出された hidebiff が自分自身を別プロセスで呼び出す際に使用するオプション。
 * @li -i: 受信直前にこのオプションで呼び出される。
 * @li -d: 秀丸メール終了時にこのオプションで呼び出される。
 * @li -s: ToastNotify をクリックしたときにこのオプションで呼び出される。
 * @li なし: 引数がない場合は About ダイアログを表示する。
 * @return 常に 0
 */
int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE,
                     LPTSTR lpCmdLine,
                     int) {
	if (strstr(lpCmdLine, "--debug") != NULL) {
		Debug = true;
	}
#ifdef _DEBUG
	Debug = true;
#endif
	debug("<Option>\n");
	debug(lpCmdLine);
	debug("\n</Option>\n");

	hInst = hInstance;
	createWindow();

	if (strstr(lpCmdLine, "-r") == lpCmdLine) {
		const char *work = getWorkDir();
		string cmd = "\"" + string(work) + "\\hidebiff.exe\" -c " + lpCmdLine;

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, HIGH_PRIORITY_CLASS, NULL, NULL, &si, &pi);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	} else {
		IniFile::hMutex = CreateMutex(NULL, FALSE, NULL);
		hMailsMutex = CreateMutex(NULL, FALSE, NULL);

		if (strstr(lpCmdLine, "-c") == lpCmdLine) {
			lpCmdLine += 3;
		}

		// ini ファイルが存在するかどうかを調査する
		// なければ初期設定してもらう
		const char *work = getWorkDir();
		string inifile = string(work) + "\\hidebiff.ini";
		if (!PathFileExists(inifile.c_str())) {
			HANDLE hMutex = CreateMutex(NULL, TRUE, "hidebiff.exe");
			if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
				// 初期設定中に起動されても何もしない
			} else {
				OPENFILENAME ofn;
				char szFile[MAX_PATH] = "";
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.lpstrFilter = TEXT("ToastNotify本体(ToastNotify.exe)\0ToastNotify.exe\0\0");
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
				ofn.lpstrTitle = "ToastNotifyの選択";

				if (GetOpenFileName(&ofn)) {
					// INIファイルオープン
					WaitForSingleObject(IniFile::hMutex, INFINITE);
					IniFile ini(inifile.c_str());

					// ToastNotify のパスを設定
					ini.write("Settings", "ToastNotify", szFile);
					ini.write("Settings", "LimitCount", "25");
					ReleaseMutex(IniFile::hMutex);
					string message = "初期設定が完了しました。\nより詳細な設定はhidebiff.mhtを参照ください。\n\n（このウィンドウはクリックするか30秒経過すると閉じます）";


					string cmd = "\"" + string(szFile) + "\"";
					cmd += " /i \"" + string(work) + "\\hidebiff.exe,0\"";
					cmd += " /m \"" + string(work) + "\\hidebiff.tnm\"";
					cmd += " /o 30";
					cmd += " /t \"初期設定完了\"";
					cmd += " /c \"" + message + "\"";

					PROCESS_INFORMATION pi;
					STARTUPINFO si;
					memset(&si, 0, sizeof(STARTUPINFO));
					si.cb = sizeof(STARTUPINFO);
					CreateProcess(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
					CloseHandle(pi.hThread);
					CloseHandle(pi.hProcess);
				}
			}
		} else {
			if (strlen(lpCmdLine) == 0) {
				HANDLE hMutex = CreateMutex(NULL, TRUE, "hidebiff.exe:about");
				if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
					HWND hwnd = FindWindow(NULL, "hidebiffについて");
					if (hwnd) {
						if (GetProp(hwnd, "hidebiff.exe:prop")) {
							SetForegroundWindow(hwnd);
						}
					}
				} else {
					DialogBox(hInst, (LPCTSTR)IDD_DIALOG1, NULL, (DLGPROC)About);
				}
			} else {
				mainLoop(lpCmdLine);
				toastNotice();
			}
		}
		CloseHandle(IniFile::hMutex);
		CloseHandle(hMailsMutex);
		if (debugout) {
			fclose(debugout);
		}
	}
	return 0;
}
