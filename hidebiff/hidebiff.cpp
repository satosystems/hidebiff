/*
 * Copyright (c) 2006-2012 Satoshi Ogata
 */

/*
�y�f�o�b�O�̕��@�z
����M�������[����������x�g�[�X�g�\���������ꍇ
1. ��M�������[���̃A�J�E���g�� ini �t�@�C����
   LogTime �̐��l��K���Ɍ��炵�ALogCount �� 1 �߂��B
2. �f�o�b�O�N���̃I�v�V�����Ɉȉ����w�肷��
    -c -r 1
   �Ō�̐����������̂ڂ��ăg�[�X�g�\�������������[���̐��B
*/

/**
 * @file
 * �A�v���P�[�V�����̃G���g�� �|�C���g����`����Ă���t�@�C���B
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

#define DELIM { 0x0c, '!', ' ', 'e', ':', '\0' } ///< ���O�t�@�C���̃f���~�^�B
#define DELIM2 { 0x0c, '!', ' ', 'r', ':', '\0' } ///< �f�R�[�h�ς݃��O�t�@�C���̃f���~�^�B

/**
 * �t�@�C���Ƀf�o�b�O���O�o�͂��s�����ۂ��B
 *
 * �f�o�b�O�r���h�\���A�܂��� --debug �������ɉ������ꍇ�� true �ɂȂ�B
 */
static bool Debug = false;

/**
 * ���̃v���Z�X�̃C���X�^���X�ϐ��B
 */
static HINSTANCE hInst;

static const char *getWorkDir(void);

/**
 * �f�o�b�O�o�͂��s�Ȃ��B
 *
 * ���� msg �� NULL �Ȃ牽���s�Ȃ�Ȃ��B
 * msg �� NULL �ł͂Ȃ��A�f�o�b�O�o�͗p�t�@�C���|�C���^ #debugout �� NULL �Ȃ�
 * #debugout ������������B
 *
 * �f�o�b�O�o�͂��s�Ȃ����ꍇ�A�v���O�����I������
 * #debugout ���N���[�Y���Ȃ���΂Ȃ�Ȃ��B
 *
 * @param [in] msg �f�o�b�O�o�̓��b�Z�[�W
 */
static FILE *debugout = NULL;
#define DEBUG_OUTPUT "hidebiff.log" ///< �f�o�b�O�o�̓t�@�C���p�X�B
#if _DEBUG
/**
 * �f�o�b�O�o�͂��s���B
 *
 * @param [in] msg �f�o�b�O���b�Z�[�W
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
 * �f�o�b�O�o�͂��s���B
 *
 * @param [in] msg �f�o�b�O���b�Z�[�W
 */
static void debug(wstring msg) {
	debug(msg.c_str());
}

/**
 * �f�o�b�O�o�͂��s���B
 *
 * @param [in] msg �f�o�b�O���b�Z�[�W
 */
static void debug(const char *msg) {
	static wchar_t dest[1024];
	size_t num;
	mbstowcs_s(&num, dest, msg, sizeof(dest));
	debug(wstring(dest));
}

/**
 * �f�o�b�O�o�͂��s���B
 *
 * @param [in] msg �f�o�b�O���b�Z�[�W
 */
static void debug(string msg) {
	debug(msg.c_str());
}

/**
 * �f�o�b�O�o�͂��s���B
 *
 * @param [in] i �f�o�b�O���b�Z�[�W
 */
static void debug(int i) {
	char buf[20] = {0};
	_itoa_s(i, buf, 10);
	debug(buf);
}

/**
 * �f�o�b�O�o�͂��s���B
 *
 * @param [in] i �f�o�b�O���b�Z�[�W
 */
static void debug(size_t i) {
	debug(static_cast<int>(i));
}
#else
#define debug(a)
#endif

/**
 * 16 �i���� 8 ���̕������ 64 bit DWORD �ɂ��ĕԂ��B
 * �t�H�[�}�b�g���s�K�؂ȏꍇ�� 0 ��Ԃ��B
 *
 * @param [in] hex 16 �i��������
 * @return ���l
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
 * �G���[���b�Z�[�W��\������B
 *
 * ���O�ɔ��������G���[���b�Z�[�W�����b�Z�[�W�{�b�N�X�ɕ\������B
 * �G���[���������Ȃ������ꍇ�́A���u�������b�Z�[�W��\������B
 *
 * @param [in] title ���b�Z�[�W�{�b�N�X�̃^�C�g��
 * @param [in] msgid ���b�Z�[�W ID
 */
static void msgbox(const char *title, DWORD msgid = 0, const char *append = NULL) {
	LPSTR lpBuffer;
	if (msgid == 0) msgid = GetLastError();
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,	// ���͌��Ə������@�̃I�v�V����
		NULL,															// ���b�Z�[�W�̓��͌�
		msgid,															// ���b�Z�[�W���ʎq
		LANG_USER_DEFAULT,												// ���ꎯ�ʎq
		(LPSTR)&lpBuffer,												// ���b�Z�[�W�o�b�t�@
		0,																// ���b�Z�[�W�o�b�t�@�̍ő�T�C�Y
		NULL															// �����̃��b�Z�[�W�}���V�[�P���X����Ȃ�z��
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
 * �w�肳�ꂽ�t�@�C���̓��e���擾����B
 *
 * �Ăяo�����Ŗ߂�l�̃|�C���^���J�����Ȃ���΂Ȃ�Ȃ��B
 * size �� NULL �̏ꍇ�́A���R�t�@�C���T�C�Y�̓|�C���^�ɏo�͂���Ȃ��B
 *
 * ���̊֐��� [�t�@�C�����e] + ['\0'] �Ƃ����`���Ń|�C���^��Ԃ��B
 * �������o�͂���� size �͍Ō�� \0 ���܂܂Ȃ��B
 *
 * �t�@�C�������݂��Ȃ��ꍇ�A�����Ɏ��s�����ꍇ�� NULL ��Ԃ��B
 *
 * @param [in] filename �t�@�C���p�X
 * @param [out] size �t�@�C���T�C�Y
 * @return �t�@�C���̓��e
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

	// �t�@�C���̐���
	file1 = CreateFile(filename, GENERIC_READ | FILE_SHARE_READ | FILE_SHARE_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
	if (file1 == INVALID_HANDLE_VALUE) {
		// �t�@�C�������݂��Ȃ�
		char append[1024];
		sprintf_s(append, "\nPath:%s", filename);
		msgbox("error 12", 0, append);
		return result;
	}

	// �T�C�Y�̎擾
	lsize1 = GetFileSize(file1, &hsize1);
	if (hsize1 != 0) {
		// �t�@�C�����傫������ꍇ�̓G���[�ɂ����Ⴄ
		msgbox("error 22");
		return result;
	}
	
	// �������}�b�v�h�t�@�C���̐���
	map1 = CreateFileMapping(file1, NULL, PAGE_READONLY, hsize1, lsize1, map.c_str());
	if (map1 == NULL) {
		// �G���[����
		msgbox("error 32");
		CloseHandle(file1);
		return result;
	}
	
	// �}�b�v�r���[�̐���
	view1 = (char *)MapViewOfFile(map1, FILE_MAP_READ, 0, 0, 0);
	if (view1 == NULL) {
		// �G���[����
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
	
	// �A���}�b�v
	if (!UnmapViewOfFile(view1)) {
		// �G���[����
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
 * INI �t�@�C����ۑ�����f�B���N�g���i���̃��W���[�����z�u����Ă���f�B���N�g���j���擾����B
 *
 * ���̊֐��͂��̊֐����� static �ϐ��̃|�C���^��Ԃ����߁A
 * �Ăяo�����̓|�C���^�̎w�����ύX���Ă͂Ȃ�Ȃ��B
 *
 * @return �f�B���N�g���p�X
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
			// �G�ۂ��C���X�g�[������Ă��Ȃ��Ȃ�A�G�ۃ��[���f�B���N�g�����z�[��
			work = (char *)get_reg_value(turukamedir, &type);
		} else {
			// �G�ۂ��C���X�g�[������Ă���Ȃ�}�N���p�X�𒲂ׂ�
			work = (char *)get_reg_value(macropath, &type);
			// �ݒ肪�u�����N�Ȃ�G�ۂ� InstallLocation ���z�[��
			if (work != NULL && work[0] == '\0') {
				free(work);
				work = NULL;
			}
			if (work == NULL) {
				// �}�N���p�X�����݂��Ȃ���ΏG�ۂ� InstallLocation ���z�[��
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
 * ���[���f�B���N�g�����擾����B
 *
 * ���̊֐��͂��̊֐����� static �ϐ��̃|�C���^��Ԃ����߁A
 * �Ăяo�����̓|�C���^�̎w�����ύX���Ă͂Ȃ�Ȃ��B
 *
 * @return ���[���f�B���N�g���B������̍Ō�� "\" ���t�^���Ă��邱�Ƃɒ���
 */
static const char *getMailDir(void) {
	static char mailDir[MAX_PATH + 1] = {'\0'};
	const char *MAIL_DIR = "HKEY_CURRENT_USER\\Software\\Hidemaruo\\TuruKame\\Config\\HomeDir";

	if (mailDir[0] == '\0') {
		DWORD type;
		// ���[���f�[�^�p�̃f�B���N�g�����擾
		char *work = (char *)get_reg_value(MAIL_DIR, &type);
		strcpy_s(mailDir, work);
		free(work);
	}
	
	return mailDir;
}


/**
 * ���[���A�J�E���g���擾����B
 *
 * �߂�l�͓K�؂ɊJ�����Ȃ���΂Ȃ�Ȃ��B
 *
 * @param [in] mailDir ���[���f�B���N�g��
 * @param [out] ���[���A�J�E���g��
 * @return ���[���A�J�E���g�̃f�B���N�g����
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
 * ���O�t�@�C������������B
 * 
 * @param [in] account �A�J�E���g��
 * @param [in] dirName ���O�t�@�C����T���f�B���N�g��
 * @param [out] logfiles �����������O�t�@�C�����i�[����R���e�i
 * @param [in] flag
 * @li 0: �t�@�C����������Ȃ��ꍇ�̓f�B���N�g����Ԃ�
 * @li 1: �ŐV�̃��O�t�@�C���݂̂�Ԃ�
 * @li 2: �͈͓��̃��O�t�@�C�����ׂĂ�Ԃ�
 * @li 3: �ŐV�̂��̓�����Ԃ��i�����ƌ����ɂ܂������Ď�M�����ꍇ�j
 */
static void findLogInfo(string account, string dirName, map<string, FILETIME> &logfiles, int flag = 0) {
	static unsigned long long itime = 0; // ���̎��Ԃ����O�̃^�C���X�^���v�̃��O�͖�������
	if (itime == 0) {
		FILETIME ignoreTime = {0};
		SYSTEMTIME st;
		GetSystemTime(&st);
		st.wHour -= 8; // 8 ���ԑO�ɍX�V���ꂽ���O�͖������Ă����Ƃ����d�l
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

	// �t�@�C������
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
 * ���O�t�@�C�������擾����B
 *
 * @return ���O�t�@�C�����ƃ^�C���X�^���v�̃}�b�v�I�u�W�F�N�g
 */
static map<string, FILETIME> getLogInfo() {
	const char *home = getMailDir();
	
	// �T�u�f�B���N�g�����擾
	int account_count;
	char **subdirs = getMailAccountDirs(home, &account_count);
	// ���O�t�@�C�����擾
	map<string, FILETIME> logfiles;
	for (int i = 0; i < account_count; i++) {
		findLogInfo(subdirs[i], "��M���O", logfiles);
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
 * ���O�t�@�C�������擾����B
 *
 * @param [in] account_count �A�J�E���g��
 * @param [in] subdirs �A�J�E���g���i�A�J�E���g�t�H���_���j
 * @return ���O�t�@�C�����ƃ^�C���X�^���v�̃}�b�v�I�u�W�F�N�g
 */
static map<string, FILETIME> getLogInfo(int account_count, char **subdirs) {
	// ���O�t�@�C�����擾
	map<string, FILETIME> logfiles;
	for (int i = 0; i < account_count; i++) {
		findLogInfo(subdirs[i], "��M���O", logfiles);
	}

	return logfiles;
}

/**
 * ���[���ŊǗ����Ă���A���[����M���ɍX�V�����\���̂���t�@�C�����X�g��Ԃ��B
 * 
 * �X�V�����\���̂���t�@�C���́A�t�@�C�����̂����Ƃ��V�������̂Q�Ƃ���B
 * 
 * @return �X�V�����\���̂���t�@�C�����X�g
 */
map<string, FILETIME> getDistributedLogInfo(void) {
	const char *home = getMailDir();
	// �T�u�f�B���N�g�����擾
	int account_count;
	char **subdirs = getMailAccountDirs(home, &account_count);

	// ���O�t�@�C�����擾
	map<string, FILETIME> logfiles;
	for (int i = 0; i < account_count; i++) {
		string findconst = string(home) + string(subdirs[i]) + "\\��M���O\\��M���O*.txt";

		HANDLE dno;
		LPCSTR dir = findconst.c_str();
		WIN32_FIND_DATA fil;
		char logfile[MAX_PATH] = {0};
		FILETIME filetime;

		// �t�@�C������
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
		string subdir = string(home) + string(subdirs[i]) + "\\��M���O\\" + logfile;
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
 * �܂��AOpenProcessToken��GetCurrentProcess�̖߂�l���w�肵�āA
 * ���݂̃v���Z�X�̃g�[�N�����擾���܂��B
 * ���̂Ƃ��AAdjustTokenPrivileges�̌Ăяo�����K�v�ɂȂ�֌W��A
 * TOKEN_ADJUST_PRIVILEGES���w�肵�Ă��܂��B
 * �����āALookupPrivilegeValue�œ���������֘A����LUID���擾���A
 * �����TOKEN_PRIVILEGES�\���̂Ɏw�肵�܂��B
 * �w�肷������̐���1�ł��邽�߁APrivilegeCount��1�ł���A
 * bEnable�ɂ�TRUE���ݒ肳��Ă��邽�߁AAttributes�ɂ�
 * SE_PRIVILEGE_ENABLED���w�肳��܂��B
 * ����ɂ��AAdjustTokenPrivileges�̌Ăяo���œ������L���ɂȂ�܂��B
 * �������A�������ӂ��Ȃ���΂Ȃ�Ȃ��̂́AAdjustTokenPrivileges�̖߂�l��
 * ���������ۂɗL���ɂł������ǂ�����\�킳�Ȃ��Ƃ����_�ł��B
 * ���̊֐��́A������\���̂̎w��ɖ�肪�Ȃ����TRUE��Ԃ����߁A
 * ������L���ɂł������ǂ�����GetLastError��ʂ��Ċm�F����K�v������܂��B
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
 * ini �t�@�C���̏��������s���B
 */
static void init(void) {
	const char *work = getWorkDir();
	const char *home = getMailDir();
	
	// �T�u�f�B���N�g�����擾
	int account_count;
	char **subdirs = getMailAccountDirs(home, &account_count);

	// ���O�t�@�C�����擾
	map<string, FILETIME> logfiles = getLogInfo(account_count, subdirs);

	// INI�t�@�C���I�[�v��
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

	// ToastNotify �̃p�X��ێ�
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

	// ��������폜���ē��e���N���A
	ini.del();

	// ToastNotify �p�X�������o��
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

	// ���[���f�B���N�g�����o��
	ini.write("Settings", "MailDir", home);

	// ��M���O�����ׂďo��
	map<string, FILETIME>::iterator it = logfiles.begin();
	for (int i = 0; it != logfiles.end(); i++, it++) {
		char buf[MAX_PATH + 1];

		strcpy_s(buf, it->first.c_str());
		int end = (int)(strstr(buf, "\\��M���O\\") - buf);
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
			// TODO: �A�J�E���g��񂪕ύX���ꂽ��Ȃ񂾂肵�āA������Ȃ�
			msgbox("ERROR 1: ������Ȃ�");
			exit(0);
		}

		// �A�J�E���g�����o��
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

		// �ŐV�̎�M���O�̃��b�Z�[�W�����J�E���g���Ă����Ȃ���΂Ȃ�Ȃ�
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
			linenum--; // �Ō�̈�s�͋󔒍s�̂��߃J�E���g���Ȃ�
			free(filter);
		}
	}
	ini.write("Settings", "FilterLog", linenum);
	ReleaseMutex(IniFile::hMutex);
}

/**
 * �e�A�J�E���g�̃t�B���^�[���B
 * 
 * map �� first �̓A�J�E���g���B
 * map �� second �ɂ͐U�蕪������\���̂���t�H���_���ꗗ�Ŏ����Ă���B
 * ���̈ꗗ�� vector �����A���e�͏d�������A�o������ێ����Ă���B
 * 
 * vector �̃t�H���_�͂��Ƃ��Έȉ��̂悤�Ȍ`���ŕێ����Ă���B
 * 
 * ��M\foo\bar
 */
static map<string, vector<string>> filters;

/**
 * �e�A�J�E���g�� "filter.txt" ��ǂݍ��񂾂��ǂ����B
 */
static map<string, bool> isReadFilter;

/**
 * "filterlog.txt" ��ǂݍ��񂾂��ǂ���������킷�t�B�[���h�B
 */
static bool isReadFilterLog = false;

/**
 * �ʒm���s��Ȃ����[���� Message-Id �̗񋓁B
 */
static vector<string> ignoreMessageId;

/**
 * ��M�������v���[�����B
 */
static int totalReceivedMail = 0;

/**
 * ���[����\���N���X�B
 */
class Mail {
#define splitToken(base)	(temp = strstr(base, "\t"), temp[0] = '\0', ++temp) ///< �^�u������؂�𕪊�����}�N���B�K���^�u�������܂܂��Ƃ����O��B
public:
	/**
	 * �R���X�g���N�^�B
	 *
	 * @param [in] account ���[���A�J�E���g��
	 * @param [in] contents �w�b�_���܂߂����[���{��
	 */
	Mail(const char *account, const char *contents) : account(account), initialized(false), ignoreNotify(false), codepage(CP_ACP) {
		char buf[20] = {'\0'};
		strncpy_s(buf, contents, _TRUNCATE);
		key = string(buf);
		key[key.find(' ')] = '\0';

		if (!isReadFilterLog) {
			isReadFilterLog = true;
			addFilter(account, "��M"); // �U�蕪���Ƀq�b�g���Ȃ��������M��

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

							char *tokenDate = line;											// ���t
							char *tokenAccount = splitToken(tokenDate);						// �A�J�E���g
							char *tokenSendOrRecv = splitToken(tokenAccount);				// ��/��
							char *token3 = splitToken(tokenSendOrRecv);						// ��
							char *tokenFromOrTo = splitToken(token3);						// From:/To:
							char *tokenMessageId = splitToken(tokenFromOrTo);				// Message-Id
							char *tokenFuriwakeJyoken;										// �U�蕪���q�b�g����
							if (strstr(tokenMessageId, "Message-Id:") != tokenMessageId) { // Message-Id �����݂��Ȃ��P�[�X
								tokenFuriwakeJyoken = tokenMessageId;
								tokenMessageId = NULL;
							} else {
								tokenFuriwakeJyoken = splitToken(tokenMessageId);
							}
							char *tokenFuriwakeSaki = NULL;									// �U�蕪����
							if (strstr(tokenFuriwakeJyoken, "\t")) {
								tokenFuriwakeSaki = splitToken(tokenFuriwakeJyoken);
							}
							char *tokenOtherInfo = NULL;									// ���̑����
							if (tokenFuriwakeSaki != NULL && strstr(tokenFuriwakeSaki, "\t")) {
								tokenOtherInfo = splitToken(tokenFuriwakeSaki);
							}

							bool skip = false;
							if (strncmp(tokenSendOrRecv + 1, "��", 2) == 0) {
								skip = true;
							}
							if (!skip) {
								if (tokenOtherInfo) {
									if (strcmp(tokenOtherInfo, "�ʒm����") == 0) {
										// FIXME: �ʒm�Ȃ��ł��� Message-Id �̂Ȃ����[���͒ʒm���Ă��܂�
										if (tokenMessageId) {
											tokenMessageId += strlen("Message-Id:");
											ignoreMessageId.push_back(string(tokenMessageId));
											skip = true;
										}
									}
								}
							}
							if (!skip) {
								if (strcmp(tokenFuriwakeJyoken, "�U�蕪������Ȃ�") == 0) {
									if (strcmp(account, tokenAccount) != 0) {
										addFilter(string(tokenAccount), "��M");
									}
									skip = true;
								}
							}

							if (!skip) {
								if (strstr(tokenFuriwakeSaki, "�ړ�:�A�J�E���g�̃o�C�p�X:") == tokenFuriwakeSaki) {
									// �ڍ׃��O�Ȃ疳�����Ă悢
								} else {
									if (strstr(tokenFuriwakeSaki, "�ړ�:") == tokenFuriwakeSaki) {
										string dist;
										dist = tokenFuriwakeSaki + strlen("�ړ�:");
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
	 * �f�X�g���N�^�B
	 * ���ɉ������Ȃ��B
	 */
	~Mail() {
	}

	/**
	 * �����ŗ^����ꂽ #text �̃_�u���R�[�e�[�V������ '\' �ŃG�X�P�[�v����B
	 *
	 * @param [in] text �Ώۂ̕�����
	 * @return �G�X�P�[�v��̕�����
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
	 * �w�b�_���擾����B
	 *
	 * @param [in] headerName �w�b�_��
	 * @return �w�b�_
	 */
	wstring getHeader(wstring &headerName) {
		return headers[headerName];
	}

	/**
	 * �w�b�_���擾����B
	 *
	 * @param [in] headerName �w�b�_��
	 * @return �w�b�_
	 */
	wstring getHeader(const wchar_t *headerName) {
		wstring name(headerName);
		return getHeader(name);
	}

	/**
	 * ���[���̖{����ݒ肷��B
	 *
	 * @param [in] aBody ���[���̖{��
	 */
	void setBody(string aBody) {
		const char *bodyBytes = aBody.c_str();
		DWORD flag = MB_ERR_INVALID_CHARS;
		int size = MultiByteToWideChar(codepage, flag, bodyBytes, -1, NULL, 0);
		if (size == 0) {
			flag = MB_PRECOMPOSED; // GB2312 �ɓ��{�ꂪ�܂܂�Ă���ꍇ�Ȃǂ� MB_ERR_INVALID_CHARS �ł̓G���[�ɂȂ邽�߂�蒼���Ă݂�
			size = MultiByteToWideChar(codepage, flag, bodyBytes, -1, NULL, 0);
		}
		wchar_t *wcs = (wchar_t *)malloc(sizeof(wchar_t) * size);
		MultiByteToWideChar(codepage, flag, bodyBytes, -1, wcs, size);

		body = wstring(wcs);
	}

	/**
	 * ���[���̖{����Ԃ��B
	 *
	 * @return ���[���̖{��
	 */
	wstring & getBody() {
		return body;
	}

	/**
	 * ���[���̃L�[��Ԃ��B
	 *
	 * @return ���[���̃L�[
	 */
	string & getKey() {
		return key;
	}

	/**
	 * ���[���̃t�@�C���p�X��Ԃ��B
	 *
	 * @return ���[���̃t�@�C���p�X
	 */
	string & getFilePath() {
		return filepath;
	}

	/**
	 * ���[���̃C���f�b�N�X��ݒ肷��B
	 *
	 * @param [in] ���[���C���f�b�N�X
	 */
	void setIndex(int aIndex) {
		index = aIndex;
	}

	/**
	 * ���[���̃t�@�C���p�X��ݒ肷��B
	 *
	 * @param [in] ���[���̃t�@�C���p�X
	 */
	void setFilePath(string aFilePath) {
		filepath = aFilePath;
	}

	/**
	 * ���[���̃C���f�b�N�X���擾����B
	 *
	 * @return ���[���̃C���f�b�N�X
	 * @note ���̃��\�b�h���Ԃ��o�b�t�@�͊J�����Ȃ��Ă��悢����Ƀ}���`�X���b�h�ɑΉ����Ă��Ȃ��B
	 */
	const char * getIndex() {
		static char buff[10];
		memset(buff, 0, sizeof(buff));
		_itoa_s(index, buff, 10);
		return buff;
	}

	/**
	 * ���[�������������ꂽ���ǂ�����Ԃ��B
	 *
	 * @return ���[�����������ς݂Ȃ� true�A�����łȂ��Ȃ� false
	 */
	bool isInitialized() {
		return initialized;
	}
	
	/**
	 * �w�b�_���p�[�X���ăw�b�_�G���g��������������B
	 *
	 * �����Ƃ��ēn����� #header �́A�w�b�_�Ɋ܂܂�� "X-Body-Content-Type"
	 * �̕����R�[�h�Z�b�g�Ńf�R�[�h����Ă��邽�߁A���̂܂܎g�p����ƕ�����������\��������B
	 * �����h�����߂̂ɁA���̃��\�b�h�͂������� Unicode �ɕϊ����Ă���w�b�_�G���g���ɒǉ�����B
	 * 
	 * @param [in] header �f�R�[�h�ς݂̃w�b�_
	 */
	void parseHeader(string &header) {
		// �w�b�_�̗]�v�ȉ��s�����ׂď���
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
		int size = MultiByteToWideChar(codepage, flag, headerBytes, -1, NULL, 0); // MB_PRECOMPOSED ���� UTF-8 �ŃG���[�ɂȂ����̂ŁA���΂炭 MB_ERR_INVALID_CHARS �Ŏ����Ă݂�
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
	 * �ʒm�𖳎����郁�[�����ǂ�����Ԃ��B
	 *
	 * @return �������郁�[���Ȃ� true�A�����łȂ��Ȃ� false
	 */
	bool isIgnoreNotify() {
		return ignoreNotify;
	}

	/**
	 * ���[���̃R�[�h�y�[�W������������B
	 *
	 * ���[���w�b�_���� "X-Body-Content-Type:" ��T���o���A
	 * �����Ɋ܂܂�Ă��� charset �����o���A���̖��O����R�[�h�y�[�W������������B
	 *
	 * @param [in] header �f�R�[�h�ς݂̃w�b�_
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
			// �����R�[�h�Z�b�g�����ɃR�[�h�y�[�W������o��
			if (charset == "gb2312" || charset == "GB2312") {
				// ������ȑ̎�
				codepage = 936;
			} else if (charset == "EUC-KR" || charset == "euc-kr") {
				// �؍���
				codepage = 949;
			} else if (charset == "utf-8" || charset == "UTF-8") {
				// UTF-8
				codepage = CP_UTF8;
			} else {
				// �f�t�H���g�͓��{��R�[�h�y�[�W
				codepage = 932;
			}
		}
	}

private:
	string account;					///< �A�J�E���g���B
	string key;						///< ���[���̃L�[�B
	map<wstring, wstring> headers;	///< �w�b�_�̈ꗗ�B
	wstring body;					///< ���[���̖{���B
	string filepath;				///< ���[���̃t�@�C���p�X�B
	int index;						///< ���[���̃C���f�b�N�X�B
	bool initialized;				///< �������ς݃t���O�B
	bool ignoreNotify;				///< �����t���O�B
	unsigned int codepage;			///< ���[���̃R�[�h�y�[�W�B

	/**
	 * �t�B���^�i#filters�j�ɐU�蕪�������ǉ�����B
	 *
	 * @param [in] tempAccount �U�蕪����A�J�E���g
	 * @param [in] dist �U�蕪����t�H���_
	 */
	void addFilter(string tempAccount, const char *dist) {
		addFilter(tempAccount, string(dist));
	}

	/**
	 * �t�B���^�i#filters�j�ɐU�蕪�������ǉ�����B
	 *
	 * @param [in] tempAccount �U�蕪����A�J�E���g
	 * @param [in] dist �U�蕪����t�H���_
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

static vector<Mail> mails;			///< ��M�������[����ێ�����x�N�^�[�B
static HANDLE hMailsMutex = NULL;	///< #mails �𑀍삷��ۂ̔r���I�u�W�F�N�g�B

/**
 * ���[��������������֐��B
 *
 * �U�蕪�����Ă���\���̂���ꏊ�� #filters ���璊�o���A
 * ���ۂɐU�蕪����ꂽ�ꏊ����肵����A#Mail �N���X�̃I�u�W�F�N�g���쐬�A
 * #mails �ɃI�u�W�F�N�g��ێ�����B
 */
static void initializeMail(void) {
	for (map<string, vector<string> >::iterator fit = filters.begin(); fit != filters.end(); fit++) {
		// ���O���J���ă��[����T��
		for (size_t i = 0; i < fit->second.size(); i++) {
			map<string, FILETIME> logfiles;
			findLogInfo(fit->first, fit->second[i], logfiles, 3);
			
			debug("<�T�����t�@�C��>\n");
			for (map<string, FILETIME>::iterator it = logfiles.begin(); it != logfiles.end(); it++) {
				debug(it->first + "\n");
				DWORD size;
				char *text = NULL;
				for (int i = 0; i < 5; i++) { // �G�ۃ��[���{�̂Ƌ������ēǂ߂Ȃ��P�[�X������̂Ń��g���C����
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
						mit->setIndex((int)(pos - text - 15)); // -15 �̓��j�[�N�L�[�̑O�ɕt�^����Ă��镶����
						mit->setFilePath(it->first);

						//////////////////////
						// ���[����ǂݏo�� //
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
						// FIXME: body �̍s���͕ύX�\��
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
							if (crln > 300) { // ���X���s�R�[�h�� \r\n �ł͂Ȃ����[�����������肷��iJIRA �Ƃ��j
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
			debug("</�T�����t�@�C��>\n");
		}
	}
}

/**
 * ���[����M��̏������s�Ȃ��B
 *
 * �ʏ�ł���΁A��M�������[�����͂��ׂẴA�J�E���g�̎�M���O�̒ǉ����Ɠ�������
 * �����Ȃ�Ȃ��ꍇ���܂܂���悤�Ɏv����B
 *
 * @li INI �t�@�C�����폜�A�����񂳂�Ă��邩������Ȃ��_���l������B
 * @li ���[���f�B���N�g���̐ݒ肪�ύX����Ă��邩������Ȃ��_���l������B
 * @li ���[���A�J�E���g���ǉ��A�ύX�A�폜����Ă��邩������Ȃ��_���l������B
 * @li ��M���O���폜�A�����񂳂�Ă��邩������Ȃ��_���l������B �� ����͖���
 *
 * @param [in] count ��M���[������
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

	// �܂��� INI �t�@�C���Ɉٕς��Ȃ����m�F����
	WaitForSingleObject(IniFile::hMutex, INFINITE);
	IniFile ini(string(work).append("\\hidebiff.ini").c_str());

	// ���[���A�J�E���g���ǉ��A�ύX�A�폜����Ă��邩������Ȃ��_���m�F
	int num;
	char **accounts = getMailAccountDirs(home, &num);

	// ���O�t�@�C�����X�V����Ă��邩�ǂ������`�F�b�N
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
		int update = 0; //< 1: ���̃t�@�C���ɒǉ���M  2: �ʂ̃t�@�C���Ɏ�M
		inivalue = ini.read(accounts[j], "LogFile", NULL);
		for (size_t n = strlen(inivalue); inivalue[n] != '\\'; n--) {
			inivalue[n] = '\0';
		}
		map<string, FILETIME>::iterator it;
		for (it = logfiles.begin(); it != logfiles.end(); it++) {
			strcpy_s(buf, it->first.c_str());
			char *pos = strstr(buf, "\\��M���O\\");
			pos[10] = '\0';
			if (strcmp(buf, inivalue) == 0) {
				break;
			}
		}
		free(inivalue);
		inivalue = NULL;
		if (it != logfiles.end()) {
			inivalue = ini.read(accounts[j], "LogFile", NULL);
			if (strcmp(inivalue, it->first.c_str()) == 0) { // ���O�t�@�C���͓���
				if (initime.dwHighDateTime != it->second.dwHighDateTime ||
						initime.dwLowDateTime != it->second.dwLowDateTime) { // ���O���Ԃ͈Ⴄ
					update = 1; // ���̃A�J�E���g�͌��̃��O�t�@�C���Ƀ��[����M���Ă���
				}
			} else {
				update = 2; // ���̃A�J�E���g�͕ʂ̃��O�t�@�C���Ƀ��[����M���Ă���
				if (initime.dwHighDateTime != 0 || initime.dwLowDateTime != 0) {
					HANDLE dno;
					LPCSTR dir = inivalue;
					WIN32_FIND_DATA fil;
					FILETIME filetime;
					filetime.dwHighDateTime = 0;
					filetime.dwLowDateTime = 0;

					// �t�@�C������
					if ((dno = FindFirstFile(dir, &fil)) != INVALID_HANDLE_VALUE) {
						if ((fil.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY) {
							filetime = fil.ftLastWriteTime;
						}
					}
					FindClose(dno);
					if (initime.dwHighDateTime != filetime.dwHighDateTime ||
							initime.dwLowDateTime != filetime.dwLowDateTime) {
						update += 1; // ���̃A�J�E���g�͌��̍ŏ��̃t�@�C�����X�V����Ă���
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
				// �ʂ̃��O�t�@�C���ɂ̂݃��[������M���Ă���ꍇ
logfile_another_case:
				// �t�@�C������
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
				strcat_s(dir, "��M���O*.txt");

				currentFileName = buf + strlen(buf);
				while (currentFileName[0] != '\\') {
					currentFileName--;
				}
				currentFileName++;

				// FAT �Ȃ�t�@�C���쐬���ɁANTFS �Ȃ�t�@�C�������Ɏ擾�ł���̂ŁA�ȉ��̃��W�b�N�Ŗ��Ȃ�
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
				// ���̃��O�t�@�C���ɒǉ���M���Ă���ꍇ
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
							// ����ȍ~�͐V�K�Ɏ�M�������[��
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
 * �E�B���h�E����ʒ����ɐݒ肷��B
 *
 * @param [in] hWnd �Ώۂ̃E�B���h�E�n���h��
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
 * ToastNotify.exe ���N�����A���[�����M��ʒm����B
 */
static void toastNotice(void) {
	int currentIndex = 1;
	const char *work = getWorkDir();
	char *inivalue;
	wchar_t buff[MAX_PATH * 2];

	// �܂��� INI �t�@�C���Ɉٕς��Ȃ����m�F����
	WaitForSingleObject(IniFile::hMutex, INFINITE);
	IniFile ini(string(work).append("\\hidebiff.ini").c_str());

	// ToastNotify �̃p�X���擾����
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
// FIXME: �������~�X���|�b�v�A�b�v���Ă��A���[�U�ɂ͎ז��Ȃ����̂悤�ȋC������
//				MessageBox(NULL, "�������~�X", "���[��������������Ă��Ȃ��̂Ŗ�������܂����B", MB_OK);
				debug("<�������~�X key=\"");
				debug(mail.getKey());
				debug("\"/>\n");
			}
			debug("<�����������[��>\n");
			debug(mail.getKey());
			debug("\n");
			debug("Subject: ");
			debug(mail.getHeader(L"subject"));
			debug("\n");
			debug("</�����������[��>\n");

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

		// �I�v�V����
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
//		cmd += L" /o 0";  // �����҂��i�f�o�b�O�p�j
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
// �ȉ��͌��ݕs�̗p�̃I�v�V����
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
 * �o�[�W�������_�C�A���O�̃��b�Z�[�W�n���h���B
 *
 * @param [in] hDlg �_�C�A���O�n���h��
 * @param [in] message ���b�Z�[�W���ʎq
 * @param [in] wParam ���b�Z�[�W���ʎq�ɂ���ĈӖ����قȂ�p�����[�^
 * @return �������s�����ꍇ�� TRUE�A�����łȂ��ꍇ�� FALSE
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
 * ���C�������B
 *
 * �R�}���h���C�������̉�͂₻�̌�̏����A
 * ���d�N�����̌㔭�v���Z�X����̏����Ϗ��Ȃǂ����̊֐��ɑ΂��čs����B
 *
 * �� ���O������킵�����A�����Ń��[�v���Ă���킯�ł͂Ȃ��B
 *
 * @param [in] lpCmdLine �R�}���h���C������
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

		// �G�ۃ��[�����őO�ʂɈړ�����
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
		if (wp == 1) { // �Z�b�V�������I�����悤�Ƃ��Ă���
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
 * ���C���G���g���|�C���g�B
 * 
 * @param [in] lpCmdLine ���̈����ɂ͐擪�ɃI�v�V�������t�^�����B
 * �I�v�V�����̈Ӗ��͈ȉ��̒ʂ�
 * @li -r: �G�ۃ��[�������[������M�����ꍇ�ɂ��̃I�v�V�����ŌĂяo�����B��ɑ��������͎�M�������[���̐��B
 * @li -c: -r �ŌĂяo���ꂽ hidebiff ���������g��ʃv���Z�X�ŌĂяo���ۂɎg�p����I�v�V�����B
 * @li -i: ��M���O�ɂ��̃I�v�V�����ŌĂяo�����B
 * @li -d: �G�ۃ��[���I�����ɂ��̃I�v�V�����ŌĂяo�����B
 * @li -s: ToastNotify ���N���b�N�����Ƃ��ɂ��̃I�v�V�����ŌĂяo�����B
 * @li �Ȃ�: �������Ȃ��ꍇ�� About �_�C�A���O��\������B
 * @return ��� 0
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

		// ini �t�@�C�������݂��邩�ǂ����𒲍�����
		// �Ȃ���Ώ����ݒ肵�Ă��炤
		const char *work = getWorkDir();
		string inifile = string(work) + "\\hidebiff.ini";
		if (!PathFileExists(inifile.c_str())) {
			HANDLE hMutex = CreateMutex(NULL, TRUE, "hidebiff.exe");
			if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
				// �����ݒ蒆�ɋN������Ă��������Ȃ�
			} else {
				OPENFILENAME ofn;
				char szFile[MAX_PATH] = "";
				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(OPENFILENAME);
				ofn.lpstrFilter = TEXT("ToastNotify�{��(ToastNotify.exe)\0ToastNotify.exe\0\0");
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = MAX_PATH;
				ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
				ofn.lpstrTitle = "ToastNotify�̑I��";

				if (GetOpenFileName(&ofn)) {
					// INI�t�@�C���I�[�v��
					WaitForSingleObject(IniFile::hMutex, INFINITE);
					IniFile ini(inifile.c_str());

					// ToastNotify �̃p�X��ݒ�
					ini.write("Settings", "ToastNotify", szFile);
					ini.write("Settings", "LimitCount", "25");
					ReleaseMutex(IniFile::hMutex);
					string message = "�����ݒ肪�������܂����B\n���ڍׂȐݒ��hidebiff.mht���Q�Ƃ��������B\n\n�i���̃E�B���h�E�̓N���b�N���邩30�b�o�߂���ƕ��܂��j";


					string cmd = "\"" + string(szFile) + "\"";
					cmd += " /i \"" + string(work) + "\\hidebiff.exe,0\"";
					cmd += " /m \"" + string(work) + "\\hidebiff.tnm\"";
					cmd += " /o 30";
					cmd += " /t \"�����ݒ芮��\"";
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
					HWND hwnd = FindWindow(NULL, "hidebiff�ɂ���");
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
