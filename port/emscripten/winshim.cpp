/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/*
 * The Win32 calls the engine still makes, for the Emscripten build.
 *
 * Files go to the POSIX layer Emscripten provides, with Windows paths turned into POSIX
 * ones and looked up without regard to case, as the game's data names are. Time comes from
 * the browser's clock, and Sleep yields to the browser through Asyncify, which is what lets
 * the game's own loop run on a page at all. The device contexts and DIB sections the
 * surfaces are built on are plain memory. Everything else answers the way a Windows machine
 * with no console, no registry entry and no second copy of the game would.
 */

#include <windows.h>
#include <io.h>

#include "language/language.h"

#include <SDL3/SDL.h>
#include <emscripten.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwctype>
#include <dirent.h>
#include <fnmatch.h>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <utime.h>
#include <vector>

namespace {

DWORD LastError = ERROR_SUCCESS;

DWORD Error_From_Errno(int error)
{
	switch (error) {
		case 0:			return(ERROR_SUCCESS);
		case ENOENT:	return(ERROR_FILE_NOT_FOUND);
		case ENOTDIR:	return(ERROR_PATH_NOT_FOUND);
		case EACCES:
		case EPERM:		return(ERROR_ACCESS_DENIED);
		case EEXIST:	return(ERROR_ALREADY_EXISTS);
		default:		return(ERROR_ACCESS_DENIED);
	}
}

void Set_Errno_Error(void)
{
	LastError = Error_From_Errno(errno);
}

// A Windows path as a POSIX one: separators turned, a drive letter dropped.
std::string Posix_Spelling(char const * path)
{
	std::string result = path != nullptr ? path : "";
	std::replace(result.begin(), result.end(), '\\', '/');
	if (result.size() >= 2 && result[1] == ':' && std::isalpha((unsigned char)result[0])) {
		result.erase(0, 2);
	}
	return(result);
}

// Finds the entry of a directory a name matches when case is ignored.
bool Match_Entry(std::string const & directory, std::string const & name, std::string & found)
{
	DIR * dir = opendir(directory.empty() ? "." : directory.c_str());
	if (dir == nullptr) {
		return(false);
	}
	bool matched = false;
	while (dirent * entry = readdir(dir)) {
		if (strcasecmp(entry->d_name, name.c_str()) == 0) {
			found = entry->d_name;
			matched = true;
			break;
		}
	}
	closedir(dir);
	return(matched);
}

/*
 * The path as the file system spells it. Each component that does not exist as written is
 * replaced by the entry that matches it without regard to case; a component nothing matches
 * is kept as written, so a path to a file about to be created still comes back whole.
 */
std::string Native_Path(char const * path)
{
	std::string const posix = Posix_Spelling(path);
	struct stat st;
	if (posix.empty() || stat(posix.c_str(), &st) == 0) {
		return(posix);
	}

	std::string result;
	size_t position = 0;
	if (posix[0] == '/') {
		result = "/";
		position = 1;
	}

	bool exists = true;
	while (position <= posix.size()) {
		size_t const slash = posix.find('/', position);
		size_t const end = slash == std::string::npos ? posix.size() : slash;
		std::string component = posix.substr(position, end - position);

		if (!component.empty()) {
			std::string const directory = result.empty() ? std::string() : result;
			std::string found;
			if (exists && component != "." && component != ".." && Match_Entry(directory.empty() ? "." : directory, component, found)) {
				component = found;
			} else {
				exists = false;
			}
			if (!result.empty() && result.back() != '/') {
				result += '/';
			}
			result += component;
		}

		if (slash == std::string::npos) {
			break;
		}
		position = slash + 1;
	}
	return(result);
}

// Seconds since 1970 as 100-nanosecond intervals since 1601.
FILETIME File_Time_From_Unix(time_t seconds, long nanoseconds = 0)
{
	unsigned long long const ticks = ((unsigned long long)seconds + 11644473600ULL) * 10000000ULL + (unsigned long long)(nanoseconds / 100);
	FILETIME result;
	result.dwLowDateTime = (DWORD)(ticks & 0xFFFFFFFFULL);
	result.dwHighDateTime = (DWORD)(ticks >> 32);
	return(result);
}

unsigned long long Ticks_Of(FILETIME const & time)
{
	return(((unsigned long long)time.dwHighDateTime << 32) | time.dwLowDateTime);
}

DWORD Attributes_From_Stat(struct stat const & st)
{
	DWORD attributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : 0;
	if ((st.st_mode & S_IWUSR) == 0) {
		attributes |= FILE_ATTRIBUTE_READONLY;
	}
	return(attributes != 0 ? attributes : FILE_ATTRIBUTE_NORMAL);
}

/*
 * Handles. Each kind the shim hands out is a heap object that starts with its kind, so one
 * CloseHandle can release any of them.
 */
enum HandleKind { HANDLE_FILE = 0x46494C45, HANDLE_FIND, HANDLE_MUTEX };

struct FileHandle
{
	HandleKind Kind;
	FILE * Stream;
	std::string Path;
};

struct FindHandle
{
	HandleKind Kind;
	std::string Directory;
	std::string Pattern;
	DIR * Dir;
};

struct MutexHandle
{
	HandleKind Kind;
};

template<class T>
T * As(HANDLE handle, HandleKind kind)
{
	if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
		return(nullptr);
	}
	T * object = (T *)handle;
	return(object->Kind == kind ? object : nullptr);
}

bool Next_Match(FindHandle * find, WIN32_FIND_DATAA * data)
{
	while (dirent * entry = readdir(find->Dir)) {
		if (fnmatch(find->Pattern.c_str(), entry->d_name, FNM_CASEFOLD) != 0) {
			continue;
		}
		std::string const full = find->Directory.empty() ? std::string(entry->d_name) : find->Directory + "/" + entry->d_name;
		struct stat st;
		if (stat(full.c_str(), &st) != 0) {
			continue;
		}
		memset(data, 0, sizeof(*data));
		data->dwFileAttributes = Attributes_From_Stat(st);
		data->ftCreationTime = File_Time_From_Unix(st.st_ctime);
		data->ftLastAccessTime = File_Time_From_Unix(st.st_atime);
		data->ftLastWriteTime = File_Time_From_Unix(st.st_mtime);
		data->nFileSizeHigh = 0;
		data->nFileSizeLow = (DWORD)st.st_size;
		strncpy(data->cFileName, entry->d_name, sizeof(data->cFileName) - 1);
		return(true);
	}
	return(false);
}

/*
 * GDI. A device context is a selection slot, and a DIB section is the pixels with the
 * header that described them. Text is not drawn: only developer overlays draw any.
 */
struct DIBObject
{
	int Width;
	int Height;
	int BitCount;
	int Pitch;
	unsigned char * Bits;
};

struct DCObject
{
	DIBObject * Selected;
};

}


/*
 * Time.
 */
DWORD GetTickCount() { return((DWORD)(unsigned long long)emscripten_get_now()); }
ULONGLONG GetTickCount64() { return((ULONGLONG)emscripten_get_now()); }
DWORD timeGetTime() { return(GetTickCount()); }
UINT timeBeginPeriod(UINT) { return(0); }
UINT timeEndPeriod(UINT) { return(0); }

// The yield to the browser that every wait in the game's loop becomes.
void Sleep(DWORD ms) { emscripten_sleep(ms); }

DWORD GetLastError() { return(LastError); }
void SetLastError(DWORD error) { LastError = error; }

BOOL QueryPerformanceCounter(LARGE_INTEGER * count)
{
	count->QuadPart = (LONGLONG)(emscripten_get_now() * 1000.0);
	return(TRUE);
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER * freq)
{
	freq->QuadPart = 1000000;
	return(TRUE);
}

static void System_Time_From_Tm(struct tm const & tm, int milliseconds, LPSYSTEMTIME time)
{
	time->wYear = (WORD)(tm.tm_year + 1900);
	time->wMonth = (WORD)(tm.tm_mon + 1);
	time->wDayOfWeek = (WORD)tm.tm_wday;
	time->wDay = (WORD)tm.tm_mday;
	time->wHour = (WORD)tm.tm_hour;
	time->wMinute = (WORD)tm.tm_min;
	time->wSecond = (WORD)tm.tm_sec;
	time->wMilliseconds = (WORD)milliseconds;
}

void GetLocalTime(LPSYSTEMTIME time)
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	time_t const seconds = tv.tv_sec;
	struct tm tm;
	localtime_r(&seconds, &tm);
	System_Time_From_Tm(tm, (int)(tv.tv_usec / 1000), time);
}

void GetSystemTime(LPSYSTEMTIME time)
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	time_t const seconds = tv.tv_sec;
	struct tm tm;
	gmtime_r(&seconds, &tm);
	System_Time_From_Tm(tm, (int)(tv.tv_usec / 1000), time);
}

void GetSystemTimeAsFileTime(LPFILETIME time)
{
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	*time = File_Time_From_Unix(tv.tv_sec, tv.tv_usec * 1000);
}

BOOL FileTimeToSystemTime(const FILETIME * file, LPSYSTEMTIME system)
{
	unsigned long long const ticks = Ticks_Of(*file);
	time_t const seconds = (time_t)(ticks / 10000000ULL - 11644473600ULL);
	struct tm tm;
	gmtime_r(&seconds, &tm);
	System_Time_From_Tm(tm, (int)((ticks / 10000ULL) % 1000ULL), system);
	return(TRUE);
}

BOOL SystemTimeToFileTime(const SYSTEMTIME * system, LPFILETIME file)
{
	struct tm tm = {};
	tm.tm_year = system->wYear - 1900;
	tm.tm_mon = system->wMonth - 1;
	tm.tm_mday = system->wDay;
	tm.tm_hour = system->wHour;
	tm.tm_min = system->wMinute;
	tm.tm_sec = system->wSecond;
	*file = File_Time_From_Unix(timegm(&tm), (long)system->wMilliseconds * 1000000L);
	return(TRUE);
}

BOOL FileTimeToLocalFileTime(const FILETIME * file, LPFILETIME local)
{
	time_t const now = time(nullptr);
	struct tm tm;
	localtime_r(&now, &tm);
	long long const ticks = (long long)Ticks_Of(*file) + (long long)tm.tm_gmtoff * 10000000LL;
	local->dwLowDateTime = (DWORD)((unsigned long long)ticks & 0xFFFFFFFFULL);
	local->dwHighDateTime = (DWORD)((unsigned long long)ticks >> 32);
	return(TRUE);
}

LONG CompareFileTime(const FILETIME * a, const FILETIME * b)
{
	unsigned long long const left = Ticks_Of(*a);
	unsigned long long const right = Ticks_Of(*b);
	return(left < right ? -1 : (left > right ? 1 : 0));
}

int GetDateFormatA(DWORD, DWORD, const SYSTEMTIME * time, LPCSTR, LPSTR out, int size)
{
	SYSTEMTIME now;
	if (time == nullptr) {
		GetLocalTime(&now);
		time = &now;
	}
	char buffer[32];
	int const length = snprintf(buffer, sizeof(buffer), "%d/%d/%04d", time->wMonth, time->wDay, time->wYear) + 1;
	if (out == nullptr || size == 0) {
		return(length);
	}
	if (size < length) {
		LastError = ERROR_ACCESS_DENIED;
		return(0);
	}
	memcpy(out, buffer, (size_t)length);
	return(length);
}

int GetTimeFormatA(DWORD, DWORD flags, const SYSTEMTIME * time, LPCSTR, LPSTR out, int size)
{
	SYSTEMTIME now;
	if (time == nullptr) {
		GetLocalTime(&now);
		time = &now;
	}
	int const hour = time->wHour % 12 == 0 ? 12 : time->wHour % 12;
	char const * const half = time->wHour < 12 ? "AM" : "PM";
	char buffer[32];
	int length;
	if ((flags & TIME_NOMINUTESORSECONDS) != 0) {
		length = snprintf(buffer, sizeof(buffer), "%d %s", hour, half) + 1;
	} else if ((flags & TIME_NOSECONDS) != 0) {
		length = snprintf(buffer, sizeof(buffer), "%d:%02d %s", hour, time->wMinute, half) + 1;
	} else {
		length = snprintf(buffer, sizeof(buffer), "%d:%02d:%02d %s", hour, time->wMinute, time->wSecond, half) + 1;
	}
	if (out == nullptr || size == 0) {
		return(length);
	}
	if (size < length) {
		LastError = ERROR_ACCESS_DENIED;
		return(0);
	}
	memcpy(out, buffer, (size_t)length);
	return(length);
}


/*
 * The process.
 */
DWORD GetCurrentThreadId() { return(1); }
DWORD GetCurrentProcessId() { return(1); }
HANDLE GetCurrentProcess() { return((HANDLE)(LONG_PTR)-1); }
BOOL IsDebuggerPresent() { return(FALSE); }
void OutputDebugStringA(LPCSTR text) { if (text != nullptr) fputs(text, stderr); }
void DebugBreak() { __builtin_trap(); }

// The game's directory, where it looks for its ui folder.
static char const ModuleFileName[] = "/opents/GAME.EXE";
static int const LanguageModule = 0x4C414E47;

HMODULE GetModuleHandleA(LPCSTR) { return(nullptr); }

DWORD GetModuleFileNameA(HMODULE module, LPSTR name, DWORD size)
{
	if (size == 0) {
		return(0);
	}
	char const * const path = module == (HMODULE)&LanguageModule ? "/opents/LANGUAGE.DLL" : ModuleFileName;
	strncpy(name, path, size - 1);
	name[size - 1] = '\0';
	return((DWORD)strlen(name));
}

// Only the language library is ever loaded, and its strings are compiled in.
HMODULE LoadLibraryA(LPCSTR name)
{
	if (name != nullptr && strcasecmp(name, "Language.dll") == 0) {
		return((HMODULE)&LanguageModule);
	}
	LastError = ERROR_FILE_NOT_FOUND;
	return(nullptr);
}

FARPROC GetProcAddress(HMODULE, LPCSTR) { return(nullptr); }
BOOL FreeLibrary(HMODULE) { return(TRUE); }

static wchar_t EmptyCommandLine[] = L"";
static char EmptyCommandLineA[] = "";
LPWSTR GetCommandLineW() { return(EmptyCommandLine); }
LPSTR GetCommandLineA() { return(EmptyCommandLineA); }
LPWSTR * CommandLineToArgvW(LPCWSTR, int * count) { if (count != nullptr) *count = 0; return(nullptr); }
HANDLE LocalFree(HANDLE) { return(nullptr); }

// Every string is UTF-8, which is the code page Windows 10 1903 and later give the game too.
UINT GetACP() { return(CP_UTF8); }
UINT GetOEMCP() { return(CP_UTF8); }

int MultiByteToWideChar(UINT, DWORD, LPCSTR text, int length, LPWSTR wide, int size)
{
	std::string const source = length < 0 ? std::string(text) + '\0' : std::string(text, (size_t)length);
	std::vector<wchar_t> result;
	for (size_t index = 0; index < source.size();) {
		unsigned char const lead = (unsigned char)source[index];
		unsigned int code = lead;
		int extra = 0;
		if (lead >= 0xF0) { code = lead & 0x07; extra = 3; }
		else if (lead >= 0xE0) { code = lead & 0x0F; extra = 2; }
		else if (lead >= 0xC0) { code = lead & 0x1F; extra = 1; }
		index++;
		for (int i = 0; i < extra && index < source.size(); i++, index++) {
			code = (code << 6) | ((unsigned char)source[index] & 0x3F);
		}
		result.push_back((wchar_t)code);
	}
	if (wide == nullptr || size == 0) {
		return((int)result.size());
	}
	if ((int)result.size() > size) {
		LastError = ERROR_ACCESS_DENIED;
		return(0);
	}
	std::copy(result.begin(), result.end(), wide);
	return((int)result.size());
}

int WideCharToMultiByte(UINT, DWORD, LPCWSTR wide, int length, LPSTR text, int size, LPCSTR, LPBOOL)
{
	size_t const count = length < 0 ? wcslen(wide) + 1 : (size_t)length;
	std::string result;
	for (size_t index = 0; index < count; index++) {
		unsigned int const code = (unsigned int)wide[index];
		if (code < 0x80) {
			result += (char)code;
		} else if (code < 0x800) {
			result += (char)(0xC0 | (code >> 6));
			result += (char)(0x80 | (code & 0x3F));
		} else if (code < 0x10000) {
			result += (char)(0xE0 | (code >> 12));
			result += (char)(0x80 | ((code >> 6) & 0x3F));
			result += (char)(0x80 | (code & 0x3F));
		} else {
			result += (char)(0xF0 | (code >> 18));
			result += (char)(0x80 | ((code >> 12) & 0x3F));
			result += (char)(0x80 | ((code >> 6) & 0x3F));
			result += (char)(0x80 | (code & 0x3F));
		}
	}
	if (text == nullptr || size == 0) {
		return((int)result.size());
	}
	if ((int)result.size() > size) {
		LastError = ERROR_ACCESS_DENIED;
		return(0);
	}
	memcpy(text, result.data(), result.size());
	return((int)result.size());
}

LPWSTR CharUpperW(LPWSTR text)
{
	for (wchar_t * c = text; c != nullptr && *c != L'\0'; c++) {
		*c = (wchar_t)towupper((wint_t)*c);
	}
	return(text);
}

DWORD FormatMessageA(DWORD, LPCVOID, DWORD error, DWORD, LPSTR buffer, DWORD size, va_list *)
{
	if (buffer == nullptr || size == 0) {
		return(0);
	}
	int const length = snprintf(buffer, size, "Error %lu", (unsigned long)error);
	return(length > 0 ? (DWORD)std::min<int>(length, (int)size - 1) : 0);
}

void GlobalMemoryStatus(LPMEMORYSTATUS status)
{
	memset(status, 0, sizeof(*status));
	status->dwLength = sizeof(*status);
	status->dwTotalPhys = 0x7FFFFFFF;
	status->dwAvailPhys = 0x7FFFFFFF;
	status->dwTotalVirtual = 0x7FFFFFFF;
	status->dwAvailVirtual = 0x7FFFFFFF;
}

UINT GetWindowsDirectoryA(LPSTR buffer, UINT size)
{
	if (size > 0) {
		strncpy(buffer, "/", size);
	}
	return(1);
}

// There is one copy of the game on a page, so a mutex is only ever created.
HANDLE CreateMutexA(void *, BOOL, LPCSTR)
{
	LastError = ERROR_SUCCESS;
	return(new MutexHandle{ HANDLE_MUTEX });
}

HANDLE OpenMutexA(DWORD, BOOL, LPCSTR) { LastError = ERROR_FILE_NOT_FOUND; return(nullptr); }
BOOL ReleaseMutex(HANDLE) { return(TRUE); }
DWORD WaitForSingleObject(HANDLE, DWORD) { return(WAIT_OBJECT_0); }

// There is one thread.
void InitializeCriticalSection(LPCRITICAL_SECTION) {}
void DeleteCriticalSection(LPCRITICAL_SECTION) {}
void EnterCriticalSection(LPCRITICAL_SECTION) {}
void LeaveCriticalSection(LPCRITICAL_SECTION) {}
void InitializeSRWLock(PSRWLOCK) {}
void AcquireSRWLockExclusive(PSRWLOCK) {}
void ReleaseSRWLockExclusive(PSRWLOCK) {}

void * _aligned_realloc(void * p, size_t size, size_t align)
{
	void * result = _aligned_malloc(size, align);
	if (result != nullptr && p != nullptr) {
		memcpy(result, p, std::min(size, malloc_usable_size(p)));
		free(p);
	}
	return(result);
}


/*
 * Files.
 */
BOOL CloseHandle(HANDLE handle)
{
	if (FileHandle * file = As<FileHandle>(handle, HANDLE_FILE)) {
		fclose(file->Stream);
		delete file;
		return(TRUE);
	}
	if (MutexHandle * mutex = As<MutexHandle>(handle, HANDLE_MUTEX)) {
		delete mutex;
		return(TRUE);
	}
	return(FALSE);
}

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD, void *, DWORD disposition, DWORD, HANDLE)
{
	std::string const path = Native_Path(name);
	struct stat st;
	bool const exists = stat(path.c_str(), &st) == 0;

	char const * mode = nullptr;
	bool const write = (access & GENERIC_WRITE) != 0;
	switch (disposition) {
		case CREATE_NEW:
			if (exists) {
				LastError = ERROR_ALREADY_EXISTS;
				return(INVALID_HANDLE_VALUE);
			}
			mode = "w+b";
			break;
		case CREATE_ALWAYS:
		case TRUNCATE_EXISTING:
			if (disposition == TRUNCATE_EXISTING && !exists) {
				LastError = ERROR_FILE_NOT_FOUND;
				return(INVALID_HANDLE_VALUE);
			}
			mode = "w+b";
			break;
		case OPEN_ALWAYS:
			mode = exists ? (write ? "r+b" : "rb") : "w+b";
			break;
		case OPEN_EXISTING:
		default:
			if (!exists) {
				LastError = ERROR_FILE_NOT_FOUND;
				return(INVALID_HANDLE_VALUE);
			}
			mode = write ? "r+b" : "rb";
			break;
	}

	FILE * stream = fopen(path.c_str(), mode);
	if (stream == nullptr) {
		Set_Errno_Error();
		return(INVALID_HANDLE_VALUE);
	}
	LastError = (disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS) && exists ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS;
	return(new FileHandle{ HANDLE_FILE, stream, path });
}

BOOL ReadFile(HANDLE handle, LPVOID buffer, DWORD size, LPDWORD read, void *)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	if (file == nullptr) {
		return(FALSE);
	}
	size_t const count = fread(buffer, 1, size, file->Stream);
	if (read != nullptr) {
		*read = (DWORD)count;
	}
	return(count == size || feof(file->Stream) ? TRUE : FALSE);
}

BOOL WriteFile(HANDLE handle, LPCVOID buffer, DWORD size, LPDWORD written, void *)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	if (file == nullptr) {
		/* The standard handles a console would have. */
		FILE * stream = handle == GetStdHandle(STD_ERROR_HANDLE) ? stderr : (handle == GetStdHandle(STD_OUTPUT_HANDLE) ? stdout : nullptr);
		if (stream == nullptr) {
			return(FALSE);
		}
		fwrite(buffer, 1, size, stream);
		if (written != nullptr) {
			*written = size;
		}
		return(TRUE);
	}
	size_t const count = fwrite(buffer, 1, size, file->Stream);
	if (written != nullptr) {
		*written = (DWORD)count;
	}
	return(count == size ? TRUE : FALSE);
}

DWORD SetFilePointer(HANDLE handle, LONG distance, LONG * high, DWORD method)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	if (file == nullptr) {
		return(INVALID_SET_FILE_POINTER);
	}
	long long offset = distance;
	if (high != nullptr) {
		offset = ((long long)*high << 32) | (unsigned long)distance;
	}
	int const whence = method == FILE_END ? SEEK_END : (method == FILE_CURRENT ? SEEK_CUR : SEEK_SET);
	if (fseeko(file->Stream, (off_t)offset, whence) != 0) {
		Set_Errno_Error();
		return(INVALID_SET_FILE_POINTER);
	}
	long long const position = (long long)ftello(file->Stream);
	if (high != nullptr) {
		*high = (LONG)(position >> 32);
	}
	return((DWORD)(position & 0xFFFFFFFFLL));
}

DWORD GetFileSize(HANDLE handle, LPDWORD high)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	struct stat st;
	if (file == nullptr || fstat(fileno(file->Stream), &st) != 0) {
		return(INVALID_FILE_SIZE);
	}
	if (high != nullptr) {
		*high = 0;
	}
	return((DWORD)st.st_size);
}

BOOL SetEndOfFile(HANDLE handle)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	if (file == nullptr) {
		return(FALSE);
	}
	fflush(file->Stream);
	return(ftruncate(fileno(file->Stream), ftello(file->Stream)) == 0 ? TRUE : FALSE);
}

BOOL FlushFileBuffers(HANDLE handle)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	return(file != nullptr && fflush(file->Stream) == 0 ? TRUE : FALSE);
}

BOOL GetFileTime(HANDLE handle, LPFILETIME creation, LPFILETIME access, LPFILETIME write)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	struct stat st;
	if (file == nullptr || fstat(fileno(file->Stream), &st) != 0) {
		return(FALSE);
	}
	if (creation != nullptr) *creation = File_Time_From_Unix(st.st_ctime);
	if (access != nullptr) *access = File_Time_From_Unix(st.st_atime);
	if (write != nullptr) *write = File_Time_From_Unix(st.st_mtime);
	return(TRUE);
}

BOOL SetFileTime(HANDLE handle, const FILETIME *, const FILETIME * access, const FILETIME * write)
{
	FileHandle * file = As<FileHandle>(handle, HANDLE_FILE);
	if (file == nullptr) {
		return(FALSE);
	}
	struct utimbuf times;
	times.actime = access != nullptr ? (time_t)(Ticks_Of(*access) / 10000000ULL - 11644473600ULL) : time(nullptr);
	times.modtime = write != nullptr ? (time_t)(Ticks_Of(*write) / 10000000ULL - 11644473600ULL) : time(nullptr);
	return(utime(file->Path.c_str(), &times) == 0 ? TRUE : FALSE);
}

BOOL DeleteFileA(LPCSTR name)
{
	if (unlink(Native_Path(name).c_str()) != 0) {
		Set_Errno_Error();
		return(FALSE);
	}
	return(TRUE);
}

BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD flags)
{
	std::string const target = Native_Path(to);
	struct stat st;
	if ((flags & MOVEFILE_REPLACE_EXISTING) == 0 && stat(target.c_str(), &st) == 0) {
		LastError = ERROR_ALREADY_EXISTS;
		return(FALSE);
	}
	if (rename(Native_Path(from).c_str(), target.c_str()) != 0) {
		Set_Errno_Error();
		return(FALSE);
	}
	return(TRUE);
}

BOOL MoveFileA(LPCSTR from, LPCSTR to) { return(MoveFileExA(from, to, 0)); }

BOOL CopyFileA(LPCSTR from, LPCSTR to, BOOL fail_if_exists)
{
	std::string const target = Native_Path(to);
	struct stat st;
	if (fail_if_exists && stat(target.c_str(), &st) == 0) {
		LastError = ERROR_ALREADY_EXISTS;
		return(FALSE);
	}
	FILE * in = fopen(Native_Path(from).c_str(), "rb");
	if (in == nullptr) {
		Set_Errno_Error();
		return(FALSE);
	}
	FILE * out = fopen(target.c_str(), "wb");
	if (out == nullptr) {
		Set_Errno_Error();
		fclose(in);
		return(FALSE);
	}
	char buffer[65536];
	size_t count;
	bool ok = true;
	while ((count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
		if (fwrite(buffer, 1, count, out) != count) {
			ok = false;
			break;
		}
	}
	fclose(in);
	fclose(out);
	return(ok ? TRUE : FALSE);
}

BOOL CreateDirectoryA(LPCSTR name, void *)
{
	if (mkdir(Native_Path(name).c_str(), 0777) != 0) {
		Set_Errno_Error();
		return(FALSE);
	}
	return(TRUE);
}

BOOL RemoveDirectoryA(LPCSTR name)
{
	if (rmdir(Native_Path(name).c_str()) != 0) {
		Set_Errno_Error();
		return(FALSE);
	}
	return(TRUE);
}

DWORD GetFileAttributesA(LPCSTR name)
{
	struct stat st;
	if (stat(Native_Path(name).c_str(), &st) != 0) {
		Set_Errno_Error();
		return(INVALID_FILE_ATTRIBUTES);
	}
	return(Attributes_From_Stat(st));
}

BOOL SetFileAttributesA(LPCSTR name, DWORD attributes)
{
	std::string const path = Native_Path(name);
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		Set_Errno_Error();
		return(FALSE);
	}
	mode_t const mode = (attributes & FILE_ATTRIBUTE_READONLY) != 0 ? (st.st_mode & ~0222) : (st.st_mode | S_IWUSR);
	return(chmod(path.c_str(), mode) == 0 ? TRUE : FALSE);
}

BOOL GetFileAttributesExA(LPCSTR name, GET_FILEEX_INFO_LEVELS, LPVOID out)
{
	struct stat st;
	if (stat(Native_Path(name).c_str(), &st) != 0) {
		Set_Errno_Error();
		return(FALSE);
	}
	WIN32_FILE_ATTRIBUTE_DATA * data = (WIN32_FILE_ATTRIBUTE_DATA *)out;
	data->dwFileAttributes = Attributes_From_Stat(st);
	data->ftCreationTime = File_Time_From_Unix(st.st_ctime);
	data->ftLastAccessTime = File_Time_From_Unix(st.st_atime);
	data->ftLastWriteTime = File_Time_From_Unix(st.st_mtime);
	data->nFileSizeHigh = 0;
	data->nFileSizeLow = (DWORD)st.st_size;
	return(TRUE);
}

HANDLE FindFirstFileA(LPCSTR name, LPWIN32_FIND_DATAA data)
{
	std::string const posix = Posix_Spelling(name);
	size_t const slash = posix.rfind('/');
	std::string directory = slash == std::string::npos ? std::string() : posix.substr(0, slash);
	std::string pattern = slash == std::string::npos ? posix : posix.substr(slash + 1);
	if (slash == 0) {
		directory = "/";
	}
	if (pattern == "*.*") {
		pattern = "*";
	}
	if (!directory.empty()) {
		directory = Native_Path(directory.c_str());
	}

	DIR * dir = opendir(directory.empty() ? "." : directory.c_str());
	if (dir == nullptr) {
		Set_Errno_Error();
		return(INVALID_HANDLE_VALUE);
	}
	FindHandle * find = new FindHandle{ HANDLE_FIND, directory, pattern, dir };
	if (!Next_Match(find, data)) {
		closedir(dir);
		delete find;
		LastError = ERROR_FILE_NOT_FOUND;
		return(INVALID_HANDLE_VALUE);
	}
	return(find);
}

BOOL FindNextFileA(HANDLE handle, LPWIN32_FIND_DATAA data)
{
	FindHandle * find = As<FindHandle>(handle, HANDLE_FIND);
	if (find == nullptr || !Next_Match(find, data)) {
		LastError = ERROR_FILE_NOT_FOUND;
		return(FALSE);
	}
	return(TRUE);
}

BOOL FindClose(HANDLE handle)
{
	FindHandle * find = As<FindHandle>(handle, HANDLE_FIND);
	if (find == nullptr) {
		return(FALSE);
	}
	closedir(find->Dir);
	delete find;
	return(TRUE);
}

DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer)
{
	char path[PATH_MAX];
	if (getcwd(path, sizeof(path)) == nullptr) {
		return(0);
	}
	DWORD const length = (DWORD)strlen(path);
	if (buffer == nullptr || size <= length) {
		return(length + 1);
	}
	memcpy(buffer, path, length + 1);
	return(length);
}

BOOL SetCurrentDirectoryA(LPCSTR name)
{
	return(chdir(Native_Path(name).c_str()) == 0 ? TRUE : FALSE);
}

DWORD GetFullPathNameA(LPCSTR name, DWORD size, LPSTR buffer, LPSTR * file_part)
{
	std::string path = Posix_Spelling(name);
	if (path.empty() || path[0] != '/') {
		char cwd[PATH_MAX];
		if (getcwd(cwd, sizeof(cwd)) != nullptr) {
			path = std::string(cwd) + (cwd[strlen(cwd) - 1] == '/' ? "" : "/") + path;
		}
	}
	if (buffer == nullptr || size <= path.size()) {
		return((DWORD)path.size() + 1);
	}
	memcpy(buffer, path.c_str(), path.size() + 1);
	if (file_part != nullptr) {
		char * const slash = strrchr(buffer, '/');
		*file_part = slash != nullptr ? slash + 1 : buffer;
	}
	return((DWORD)path.size());
}

BOOL GetDiskFreeSpaceExA(LPCSTR, PULARGE_INTEGER available, PULARGE_INTEGER total, PULARGE_INTEGER free)
{
	// The browser's storage quota is not the game's to ask after; a gigabyte is plenty.
	ULONGLONG const bytes = 1024ULL * 1024ULL * 1024ULL;
	if (available != nullptr) available->QuadPart = bytes;
	if (total != nullptr) total->QuadPart = bytes;
	if (free != nullptr) free->QuadPart = bytes;
	return(TRUE);
}


/*
 * The window, the keyboard and the console.
 */
int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
	fprintf(stderr, "[%s] %s\n", caption != nullptr ? caption : "", text != nullptr ? text : "");
	if ((type & 0xF) == MB_YESNO) {
		return(IDYES);
	}
	return(IDOK);
}

// The keys SDL says are down, by their Windows virtual-key code.
SHORT GetKeyState(int virtualkey)
{
	SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
	switch (virtualkey) {
		case VK_LSHIFT:		scancode = SDL_SCANCODE_LSHIFT; break;
		case VK_RSHIFT:		scancode = SDL_SCANCODE_RSHIFT; break;
		case VK_LCONTROL:	scancode = SDL_SCANCODE_LCTRL; break;
		case VK_RCONTROL:	scancode = SDL_SCANCODE_RCTRL; break;
		case VK_LMENU:		scancode = SDL_SCANCODE_LALT; break;
		case VK_RMENU:		scancode = SDL_SCANCODE_RALT; break;
		default:			return(0);
	}
	bool const * const state = SDL_GetKeyboardState(nullptr);
	return(state != nullptr && state[scancode] ? (SHORT)0x8000 : 0);
}

SHORT GetAsyncKeyState(int virtualkey) { return(GetKeyState(virtualkey)); }
UINT MapVirtualKeyA(UINT, UINT) { return(0); }
UINT MapVirtualKeyExA(UINT, UINT, HKL) { return(0); }
UINT MapVirtualKeyExW(UINT, UINT, HKL) { return(0); }
HKL GetKeyboardLayout(DWORD) { return(nullptr); }

int GetSystemMetrics(int index)
{
	switch (index) {
		case SM_CXDOUBLECLK:
		case SM_CYDOUBLECLK:	return(4);
		case SM_CXDRAG:
		case SM_CYDRAG:			return(4);
		default:				return(0);
	}
}

UINT GetDoubleClickTime() { return(500); }
HWND GetCapture() { return(nullptr); }
HWND GetConsoleWindow() { return(nullptr); }
BOOL ShowWindow(HWND, int) { return(FALSE); }
HWND FindWindowA(LPCSTR, LPCSTR) { return(nullptr); }
BOOL SetForegroundWindow(HWND) { return(FALSE); }
BOOL InvalidateRect(HWND, const RECT *, BOOL) { return(TRUE); }
HWND SetFocus(HWND) { return(nullptr); }
HWND SetCapture(HWND) { return(nullptr); }
BOOL ReleaseCapture() { return(TRUE); }
LRESULT DefWindowProcW(HWND, UINT, WPARAM, LPARAM) { return(0); }
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return(0); }
void InitCommonControls() {}
HMENU GetSystemMenu(HWND, BOOL) { return(nullptr); }
BOOL DeleteMenu(HMENU, UINT, UINT) { return(FALSE); }

BOOL SetRect(LPRECT rect, int left, int top, int right, int bottom)
{
	rect->left = left;
	rect->top = top;
	rect->right = right;
	rect->bottom = bottom;
	return(TRUE);
}

BOOL IntersectRect(LPRECT out, const RECT * a, const RECT * b)
{
	out->left = std::max(a->left, b->left);
	out->top = std::max(a->top, b->top);
	out->right = std::min(a->right, b->right);
	out->bottom = std::min(a->bottom, b->bottom);
	if (out->left >= out->right || out->top >= out->bottom) {
		SetRect(out, 0, 0, 0, 0);
		return(FALSE);
	}
	return(TRUE);
}

// The page's console stands in for the one a debug build allocates.
BOOL AllocConsole() { return(FALSE); }
BOOL FreeConsole() { return(TRUE); }
BOOL SetConsoleTitleA(LPCSTR) { return(TRUE); }
BOOL SetConsoleOutputCP(UINT) { return(TRUE); }
BOOL SetConsoleCP(UINT) { return(TRUE); }
HANDLE GetStdHandle(DWORD which) { return((HANDLE)(ULONG_PTR)(0x100 + (which & 0xFF))); }
BOOL SetStdHandle(DWORD, HANDLE) { return(TRUE); }
BOOL GetConsoleScreenBufferInfo(HANDLE, CONSOLE_SCREEN_BUFFER_INFO *) { return(FALSE); }
BOOL SetConsoleScreenBufferSize(HANDLE, COORD) { return(FALSE); }
BOOL SetConsoleTextAttribute(HANDLE, WORD) { return(TRUE); }

BOOL WriteConsoleA(HANDLE, const void * text, DWORD length, LPDWORD written, LPVOID)
{
	fwrite(text, 1, length, stdout);
	if (written != nullptr) {
		*written = length;
	}
	return(TRUE);
}


/*
 * Resources and the registry.
 */
namespace {

struct LanguageString
{
	unsigned int ID;
	char const * Text;
};

LanguageString const LanguageStrings[] = {
#include "opents_language_table.inc"
};

}

int LoadStringA(HINSTANCE, UINT id, LPSTR buffer, int size)
{
	for (LanguageString const & entry : LanguageStrings) {
		if (entry.ID == id) {
			if (size <= 0) {
				return(0);
			}
			strncpy(buffer, entry.Text, (size_t)size - 1);
			buffer[size - 1] = '\0';
			return((int)strlen(buffer));
		}
	}
	return(0);
}

DWORD GetFileVersionInfoSizeA(LPCSTR, LPDWORD handle) { if (handle != nullptr) *handle = 0; return(0); }
BOOL GetFileVersionInfoA(LPCSTR, DWORD, DWORD, LPVOID) { return(FALSE); }
BOOL VerQueryValueA(LPCVOID, LPCSTR, LPVOID *, UINT * length) { if (length != nullptr) *length = 0; return(FALSE); }
LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, PHKEY) { return(ERROR_FILE_NOT_FOUND); }
LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) { return(ERROR_FILE_NOT_FOUND); }
LONG RegCloseKey(HKEY) { return(ERROR_SUCCESS); }


/*
 * GDI.
 */
HDC GetDC(HWND) { return(nullptr); }
int ReleaseDC(HWND, HDC) { return(1); }
HDC CreateCompatibleDC(HDC) { return((HDC)new DCObject{ nullptr }); }
BOOL DeleteDC(HDC dc) { delete (DCObject *)dc; return(TRUE); }

HGDIOBJ SelectObject(HDC dc, HGDIOBJ object)
{
	if (dc == nullptr) {
		return(nullptr);
	}
	DCObject * const context = (DCObject *)dc;
	DIBObject * const previous = context->Selected;
	// Fonts are selected too, and have no object behind them.
	if (object != nullptr && *(int const *)object == 0x44494221) {
		context->Selected = (DIBObject *)((int *)object + 1);
	}
	return(previous != nullptr ? (HGDIOBJ)((int *)previous - 1) : nullptr);
}

/*
 * A DIB section is allocated behind a tag, so SelectObject and DeleteObject can tell it from
 * the null handle a font is.
 */
HBITMAP CreateDIBSection(HDC, const BITMAPINFO * info, UINT, void ** bits, HANDLE, DWORD)
{
	int const width = info->bmiHeader.biWidth;
	int const height = info->bmiHeader.biHeight < 0 ? -info->bmiHeader.biHeight : info->bmiHeader.biHeight;
	int const bitcount = info->bmiHeader.biBitCount;
	int const pitch = ((width * bitcount + 31) / 32) * 4;
	if (width <= 0 || height <= 0) {
		return(nullptr);
	}

	int * const block = (int *)malloc(sizeof(int) + sizeof(DIBObject));
	if (block == nullptr) {
		return(nullptr);
	}
	block[0] = 0x44494221;
	DIBObject * const dib = (DIBObject *)(block + 1);
	dib->Width = width;
	dib->Height = height;
	dib->BitCount = bitcount;
	dib->Pitch = pitch;
	dib->Bits = (unsigned char *)calloc((size_t)pitch, (size_t)height);
	if (dib->Bits == nullptr) {
		free(block);
		return(nullptr);
	}
	if (bits != nullptr) {
		*bits = dib->Bits;
	}
	return((HBITMAP)block);
}

BOOL DeleteObject(HGDIOBJ object)
{
	if (object != nullptr && *(int const *)object == 0x44494221) {
		DIBObject * const dib = (DIBObject *)((int *)object + 1);
		free(dib->Bits);
		free(object);
	}
	return(TRUE);
}

int GetObjectA(HANDLE object, int size, LPVOID out)
{
	if (object == nullptr || *(int const *)object != 0x44494221 || size < (int)sizeof(DIBSECTION)) {
		return(0);
	}
	DIBObject const * const dib = (DIBObject const *)((int const *)object + 1);
	DIBSECTION * const section = (DIBSECTION *)out;
	memset(section, 0, sizeof(*section));
	section->dsBm.bmWidth = dib->Width;
	section->dsBm.bmHeight = dib->Height;
	section->dsBm.bmWidthBytes = dib->Pitch;
	section->dsBm.bmPlanes = 1;
	section->dsBm.bmBitsPixel = (WORD)dib->BitCount;
	section->dsBm.bmBits = dib->Bits;
	section->dsBmih.biSize = sizeof(BITMAPINFOHEADER);
	section->dsBmih.biWidth = dib->Width;
	section->dsBmih.biHeight = dib->Height;
	section->dsBmih.biPlanes = 1;
	section->dsBmih.biBitCount = (WORD)dib->BitCount;
	return((int)sizeof(DIBSECTION));
}

BOOL GdiFlush() { return(TRUE); }

// Nearest-neighbour, as COLORONCOLOR is.
BOOL StretchBlt(HDC to, int dx, int dy, int dw, int dh, HDC from, int sx, int sy, int sw, int sh, DWORD)
{
	if (to == nullptr || from == nullptr) {
		return(FALSE);
	}
	DIBObject const * const dest = ((DCObject *)to)->Selected;
	DIBObject const * const source = ((DCObject *)from)->Selected;
	if (dest == nullptr || source == nullptr || dest->BitCount != source->BitCount || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) {
		return(FALSE);
	}
	int const bytes = dest->BitCount / 8;
	for (int y = 0; y < dh; y++) {
		int const ty = dy + y;
		int const fy = sy + (int)((long long)y * sh / dh);
		if (ty < 0 || ty >= dest->Height || fy < 0 || fy >= source->Height) {
			continue;
		}
		unsigned char * const row = dest->Bits + (size_t)ty * dest->Pitch;
		unsigned char const * const srow = source->Bits + (size_t)fy * source->Pitch;
		for (int x = 0; x < dw; x++) {
			int const tx = dx + x;
			int const fx = sx + (int)((long long)x * sw / dw);
			if (tx < 0 || tx >= dest->Width || fx < 0 || fx >= source->Width) {
				continue;
			}
			memcpy(row + tx * bytes, srow + fx * bytes, (size_t)bytes);
		}
	}
	return(TRUE);
}

BOOL BitBlt(HDC to, int dx, int dy, int w, int h, HDC from, int sx, int sy, DWORD rop)
{
	return(StretchBlt(to, dx, dy, w, h, from, sx, sy, w, h, rop));
}

HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) { return(nullptr); }
HFONT CreateFontIndirectA(const LOGFONTA *) { return(nullptr); }

BOOL GetTextMetricsA(HDC, TEXTMETRICA * metrics)
{
	memset(metrics, 0, sizeof(*metrics));
	metrics->tmHeight = 12;
	metrics->tmAscent = 10;
	metrics->tmDescent = 2;
	metrics->tmAveCharWidth = 6;
	metrics->tmMaxCharWidth = 8;
	return(TRUE);
}

BOOL GetTextExtentPoint32A(HDC, LPCSTR text, int length, LPSIZE size)
{
	size->cx = 6 * (text != nullptr ? length : 0);
	size->cy = 12;
	return(TRUE);
}

BOOL GetTextExtentPoint32W(HDC, LPCWSTR text, int length, LPSIZE size)
{
	size->cx = 6 * (text != nullptr ? length : 0);
	size->cy = 12;
	return(TRUE);
}

BOOL TextOutA(HDC, int, int, LPCSTR, int) { return(TRUE); }
BOOL TextOutW(HDC, int, int, LPCWSTR, int) { return(TRUE); }
BOOL ExtTextOutW(HDC, int, int, UINT, const RECT *, LPCWSTR, UINT, const INT *) { return(TRUE); }
COLORREF SetTextColor(HDC, COLORREF) { return(0); }
COLORREF SetBkColor(HDC, COLORREF) { return(0); }
int SetBkMode(HDC, int) { return(OPAQUE); }
UINT SetTextAlign(HDC, UINT) { return(0); }
int SetGraphicsMode(HDC, int) { return(1); }
BOOL ModifyWorldTransform(HDC, const void *, DWORD) { return(TRUE); }
int SetStretchBltMode(HDC, int) { return(COLORONCOLOR); }
int SaveDC(HDC) { return(1); }
BOOL RestoreDC(HDC, int) { return(TRUE); }
BOOL SetViewportOrgEx(HDC, int, int, LPPOINT) { return(TRUE); }
BOOL SetWindowOrgEx(HDC, int, int, LPPOINT) { return(TRUE); }
BOOL DPtoLP(HDC, LPPOINT, int) { return(TRUE); }

