#include "..\hidebiff\hidebiff.h"
#include "..\hidebiff\IniFile.h"
#include "..\hidebiff\registory.h"

HANDLE IniFile::hMutex = NULL;

static HINSTANCE tkInfo;
static std::string macrodir;	///< 最後の \ は付与されない


void (_cdecl * EnvChanged)(int fReloadAll);
int (_cdecl * RecvMailCountShow)();
int (_cdecl * GetTransmitCommandCode)();
const char * (_cdecl * Account)(int accountNo);
int (_cdecl * LoadAccountProp)(const char *account);
int (_cdecl * SetAccountProp)(const char *propName, int value);
int (_cdecl * SaveAccountProp)();


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
		// アンロードされないようにロックしてしまう
		char szFileName[MAX_PATH];
		GetModuleFileName(hModule, szFileName, sizeof(szFileName));
		LoadLibrary(szFileName);

		GetModuleFileName(hModule, szFileName, sizeof(szFileName));
		for (size_t i = strlen(szFileName) - 1; i > 0; --i) {
			if (szFileName[i] == '\\') {
				szFileName[i] = '\0';
				break;
			}
			szFileName[i] = '\0';
		}
		macrodir = szFileName;

		tkInfo = LoadLibrary("TKInfo.dll");
		*(FARPROC *)&EnvChanged = GetProcAddress(tkInfo, "EnvChanged");
		*(FARPROC *)&RecvMailCountShow = GetProcAddress(tkInfo, "RecvMailCountShow");
		*(FARPROC *)&GetTransmitCommandCode = GetProcAddress(tkInfo, "GetTransmitCommandCode");
		*(FARPROC *)&Account = GetProcAddress(tkInfo, "Account");
		*(FARPROC *)&LoadAccountProp = GetProcAddress(tkInfo, "LoadAccountProp");
		*(FARPROC *)&SetAccountProp = GetProcAddress(tkInfo, "SetAccountProp");
		*(FARPROC *)&SaveAccountProp = GetProcAddress(tkInfo, "SaveAccountProp");
	} else if (ul_reason_for_call == DLL_THREAD_ATTACH) {
	} else if (ul_reason_for_call == DLL_THREAD_DETACH) {
	} else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
		FreeLibrary(tkInfo);
	}

    return TRUE;
}

extern "C" void _cdecl AtEnd() {
	WaitForSingleObject(IniFile::hMutex, INFINITE);
	IniFile ini((macrodir + "\\hidebiff.ini").c_str());
	char *filterLogSave = ini.read("Settings", "FilterLogSave");
	ReleaseMutex(IniFile::hMutex);
	if (strcmp(filterLogSave, "1") != 0) {
		std::string path;
		path.append("\"").append(macrodir).append("\\hidebiff.exe\" -d");

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		CreateProcess(NULL, (LPSTR)path.c_str(), NULL, NULL, FALSE, HIGH_PRIORITY_CLASS, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	free(filterLogSave);
}

extern "C" void _cdecl AtReceived() {
	std::string path;

	DWORD filterLog = 0;
	set_reg_value(FILTER_LOG_KEY, REG_DWORD, (CONST BYTE *)&filterLog, sizeof DWORD);

	EnvChanged(0);
	int count = RecvMailCountShow();
	if (count > 0) {
		char countstr[6];
		_itoa_s(count, countstr, sizeof(countstr), 10);

		WaitForSingleObject(IniFile::hMutex, INFINITE);
		IniFile ini((macrodir + "\\hidebiff.ini").c_str());
		char *limitCount = ini.read("Settings", "LimitCount");
		int limit = atoi(limitCount);
		free(limitCount);
		if (count <= limit) {
			path.append("\"").append(macrodir).append("\\hidebiff.exe\" -r ").append(countstr);
		} else {
			char *turuKameDir = (char *)get_reg_value(HIDEMARU_MAIL_DIR_KEY, NULL);
			std::string mailPath;
			mailPath.append(turuKameDir).append("TuruKame.exe");
			free(turuKameDir);

			char *icon = ini.read("ToastNotify", "icon", (macrodir + "\\hidebiff.exe,0").c_str());
			char *module = ini.read("ToastNotify", "module", (macrodir + "\\hidebiff.tnm").c_str());
			char *effect = ini.read("ToastNotify", "effect");
			char *position = ini.read("ToastNotify", "position");
			char *visibirity = ini.read("ToastNotify", "visibirity");
			char *timeout = ini.read("ToastNotify", "timeout", "5");
			char *alarm = ini.read("ToastNotify", "alarm");
			char *toastNotify = ini.read("Settings", "ToastNotify");
			
			path.append("\"").append(toastNotify).append("\"");
			path.append(" /i \"").append(icon).append("\"");
			path.append(" /m \"").append(module).append("\"");
			path.append(" /o ").append(timeout);
			path.append(" /t \"").append(countstr).append("件 受信しました。\"");
			path.append(" /c \"秀丸メール本体で受信したメールを確認してください。\"");
			path.append(" /p \"").append(mailPath).append("\"");
			path.append(" /pa \"/xhidebiff.mac\"");
			if (strlen(effect)) {
				path.append(" /e ").append(effect);
			}
			if (strlen(position)) {
				path.append(" /ps ").append(position);
			}
			if (strlen(visibirity)) {
				path.append(" /v ").append(visibirity);
			}
			if (strlen(alarm)) {
				path.append(" /a \"").append(alarm).append("\"");
			}
			free(icon);
			free(module);
			free(effect);
			free(position);
			free(visibirity);
			free(timeout);
			free(alarm);
			free(toastNotify);
		}
		ReleaseMutex(IniFile::hMutex);

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		CreateProcess(NULL, (LPSTR)path.c_str(), NULL, NULL, FALSE, HIGH_PRIORITY_CLASS, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}

extern "C" void _cdecl AtSendReceive() {
	int n = GetTransmitCommandCode();
	if (n == 40003 ||			// 受信
			n == 40216 ||		// 送受信
			n == 40024 ||		// すべて送受信
			n == 40143 ||		// すべて受信
			n == 40074 ||		// リモートメール
			n == 40209 ||		// リモートメール - 現在メールの再受信
			n == 1) {			// 定期受信
		int i = 0;
		while (true) {
			std::string account(Account(i));
			if (account == "") {
				break;
			}
			LoadAccountProp(account.c_str());
			SetAccountProp("fRecvLog", 1);
			SaveAccountProp();
			i++;
		}
		DWORD filterLog = 2;
		set_reg_value(FILTER_LOG_KEY, REG_DWORD, (CONST BYTE *)&filterLog, sizeof DWORD);
		EnvChanged(0);

		std::string path;
		path.append("\"").append(macrodir).append("\\hidebiff.exe\" -i");

		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		CreateProcess(NULL, (LPSTR)path.c_str(), NULL, NULL, FALSE, HIGH_PRIORITY_CLASS, NULL, NULL, &si, &pi);
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
}
