#pragma once
/*
 * Win32 compatibility layer for the Emscripten build of OpenTS.
 *
 * The engine still names Win32 types, constants and a small set of calls throughout. This
 * header supplies them for a target that has no Windows SDK: the types with their Win32
 * widths (wasm32 is ILP32, so long and pointers are 32 bits as on Win32), the constants with
 * their Win32 values, and declarations for the calls, which port/emscripten/winshim.cpp
 * implements on POSIX and SDL or reduces to the answer a headless Windows box would give.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#ifndef _MAX_PATH
#define _MAX_DRIVE 3
#define _MAX_DIR   256
#define _MAX_FNAME 256
#define _MAX_EXT   256
#define _MAX_PATH  512
#define MAX_PATH   _MAX_PATH
#endif

#define WINAPI
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define IN
#define OUT
#define OPTIONAL
#define __stdcall
#define _stdcall
#define __cdecl
#define _cdecl
#define __fastcall
#define __forceinline inline __attribute__((always_inline))
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _In_z_
#define _Printf_format_string_
#define _Check_return_

/* Types. */
typedef int BOOL;
typedef int INT;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef short SHORT;
typedef unsigned short USHORT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef wchar_t WCHAR;
typedef char TCHAR;
typedef float FLOAT;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef unsigned long long DWORD64;
typedef unsigned int UINT32;
typedef int INT32;
typedef long long __int64;
typedef void VOID;
typedef void * PVOID;
typedef void * LPVOID;
typedef const void * LPCVOID;
typedef char * LPSTR;
typedef const char * LPCSTR;
typedef char * LPTSTR;
typedef const char * LPCTSTR;
typedef wchar_t * LPWSTR;
typedef const wchar_t * LPCWSTR;
typedef BYTE * LPBYTE;
typedef BYTE * PBYTE;
typedef DWORD * LPDWORD;
typedef DWORD * PDWORD;
typedef LONG * LPLONG;
typedef WORD * LPWORD;
typedef BOOL * LPBOOL;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef LONG HRESULT;
typedef DWORD COLORREF;
typedef WORD ATOM;

#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ * name
typedef void * HANDLE;
typedef HANDLE * LPHANDLE;
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HBITMAP);
DECLARE_HANDLE(HFONT);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HPEN);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HKL);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HRGN);
typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;
typedef void * HGDIOBJ;
typedef HKEY * PHKEY;

typedef INT_PTR (*FARPROC)();
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#define TRUE  1
#define FALSE 0
#define CONST const

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)

#define MAKEWORD(a, b) ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xff)) | ((WORD)((BYTE)((DWORD_PTR)(b) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)((DWORD_PTR)(w) >> 8))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) (LOBYTE(rgb))
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) (LOBYTE((rgb) >> 16))
#define TEXT(s) s
#define _T(s) s
#define MAKEINTRESOURCE(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define UNREFERENCED_PARAMETER(P) (void)(P)
#define ZeroMemory(d, n) memset((d), 0, (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))
#define MoveMemory(d, s, n) memmove((d), (s), (n))
#define FillMemory(d, n, v) memset((d), (v), (n))
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define S_OK ((HRESULT)0)
#define E_FAIL ((HRESULT)0x80004005L)

/* Structures. */
typedef struct tagPOINT { LONG x; LONG y; } POINT, *LPPOINT, *PPOINT;
typedef struct tagSIZE { LONG cx; LONG cy; } SIZE, *LPSIZE;
typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT, *LPRECT;
typedef const RECT * LPCRECT;
typedef struct _COORD { SHORT X; SHORT Y; } COORD;
typedef struct _FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME {
	WORD wYear; WORD wMonth; WORD wDayOfWeek; WORD wDay;
	WORD wHour; WORD wMinute; WORD wSecond; WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;
typedef union _LARGE_INTEGER {
	struct { DWORD LowPart; LONG HighPart; };
	struct { DWORD LowPart; LONG HighPart; } u;
	LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER {
	struct { DWORD LowPart; DWORD HighPart; };
	struct { DWORD LowPart; DWORD HighPart; } u;
	ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _WIN32_FIND_DATAA {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	CHAR cFileName[MAX_PATH];
	CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA, WIN32_FIND_DATA, *LPWIN32_FIND_DATA;

typedef struct _WIN32_FILE_ATTRIBUTE_DATA {
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA;
typedef enum _GET_FILEEX_INFO_LEVELS { GetFileExInfoStandard } GET_FILEEX_INFO_LEVELS;

typedef struct tagRGBQUAD { BYTE rgbBlue; BYTE rgbGreen; BYTE rgbRed; BYTE rgbReserved; } RGBQUAD;
typedef struct tagPALETTEENTRY { BYTE peRed; BYTE peGreen; BYTE peBlue; BYTE peFlags; } PALETTEENTRY, *LPPALETTEENTRY;
typedef struct tagBITMAPINFOHEADER {
	DWORD biSize; LONG biWidth; LONG biHeight; WORD biPlanes; WORD biBitCount;
	DWORD biCompression; DWORD biSizeImage; LONG biXPelsPerMeter; LONG biYPelsPerMeter;
	DWORD biClrUsed; DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagBITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO, *LPBITMAPINFO;
typedef struct tagBITMAP {
	LONG bmType; LONG bmWidth; LONG bmHeight; LONG bmWidthBytes; WORD bmPlanes; WORD bmBitsPixel; LPVOID bmBits;
} BITMAP;
typedef struct tagDIBSECTION {
	BITMAP dsBm; BITMAPINFOHEADER dsBmih; DWORD dsBitfields[3]; HANDLE dshSection; DWORD dsOffset;
} DIBSECTION;
typedef struct tagLOGFONTA {
	LONG lfHeight; LONG lfWidth; LONG lfEscapement; LONG lfOrientation; LONG lfWeight;
	BYTE lfItalic; BYTE lfUnderline; BYTE lfStrikeOut; BYTE lfCharSet; BYTE lfOutPrecision;
	BYTE lfClipPrecision; BYTE lfQuality; BYTE lfPitchAndFamily; CHAR lfFaceName[32];
} LOGFONTA, LOGFONT;
typedef struct tagTEXTMETRICA {
	LONG tmHeight; LONG tmAscent; LONG tmDescent; LONG tmInternalLeading; LONG tmExternalLeading;
	LONG tmAveCharWidth; LONG tmMaxCharWidth; LONG tmWeight; LONG tmOverhang;
	LONG tmDigitizedAspectX; LONG tmDigitizedAspectY; BYTE tmFirstChar; BYTE tmLastChar;
	BYTE tmDefaultChar; BYTE tmBreakChar; BYTE tmItalic; BYTE tmUnderlined; BYTE tmStruckOut;
	BYTE tmPitchAndFamily; BYTE tmCharSet;
} TEXTMETRICA, TEXTMETRIC;
typedef struct _MEMORYSTATUS {
	DWORD dwLength; DWORD dwMemoryLoad; SIZE_T dwTotalPhys; SIZE_T dwAvailPhys;
	SIZE_T dwTotalPageFile; SIZE_T dwAvailPageFile; SIZE_T dwTotalVirtual; SIZE_T dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;
typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG, *LPMSG;
typedef struct _SMALL_RECT { SHORT Left; SHORT Top; SHORT Right; SHORT Bottom; } SMALL_RECT;
typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
	COORD dwSize; COORD dwCursorPosition; WORD wAttributes; SMALL_RECT srWindow; COORD dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;
typedef struct _OSVERSIONINFOW {
	DWORD dwOSVersionInfoSize; DWORD dwMajorVersion; DWORD dwMinorVersion; DWORD dwBuildNumber;
	DWORD dwPlatformId; WCHAR szCSDVersion[128];
} RTL_OSVERSIONINFOW, *PRTL_OSVERSIONINFOW, OSVERSIONINFOW;
typedef struct _CRITICAL_SECTION { void * impl; } CRITICAL_SECTION, *LPCRITICAL_SECTION;
typedef struct _RTL_SRWLOCK { void * Ptr; } SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT {0}

typedef struct _IMAGE_DOS_HEADER { WORD e_magic; WORD e_unused[29]; LONG e_lfanew; } IMAGE_DOS_HEADER;
typedef struct _IMAGE_FILE_HEADER {
	WORD Machine; WORD NumberOfSections; DWORD TimeDateStamp; DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols; WORD SizeOfOptionalHeader; WORD Characteristics;
} IMAGE_FILE_HEADER;
typedef struct _IMAGE_OPTIONAL_HEADER32 {
	WORD Magic; BYTE MajorLinkerVersion; BYTE MinorLinkerVersion; DWORD SizeOfCode;
	DWORD SizeOfInitializedData; DWORD SizeOfUninitializedData; DWORD AddressOfEntryPoint;
	DWORD BaseOfCode; DWORD BaseOfData; DWORD ImageBase; DWORD SectionAlignment; DWORD FileAlignment;
	WORD MajorOperatingSystemVersion; WORD MinorOperatingSystemVersion; WORD MajorImageVersion;
	WORD MinorImageVersion; WORD MajorSubsystemVersion; WORD MinorSubsystemVersion;
	DWORD Win32VersionValue; DWORD SizeOfImage; BYTE rest[164];
} IMAGE_OPTIONAL_HEADER32;
typedef struct _IMAGE_NT_HEADERS32 {
	DWORD Signature; IMAGE_FILE_HEADER FileHeader; IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} IMAGE_NT_HEADERS32, IMAGE_NT_HEADERS;
#define IMAGE_DOS_SIGNATURE 0x5A4D
#define IMAGE_NT_SIGNATURE 0x00004550
#define IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b

/* Constants. */
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7
#define MB_OK 0x0
#define MB_OKCANCEL 0x1
#define MB_YESNO 0x4
#define MB_ICONERROR 0x10
#define MB_ICONSTOP 0x10
#define MB_ICONQUESTION 0x20
#define MB_ICONWARNING 0x30
#define MB_ICONEXCLAMATION 0x30
#define MB_ICONINFORMATION 0x40
#define MB_TASKMODAL 0x2000
#define MB_SETFOREGROUND 0x10000
#define MB_TOPMOST 0x40000

#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 0x1
#define FILE_SHARE_WRITE 0x2
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define FILE_ATTRIBUTE_READONLY 0x1
#define FILE_ATTRIBUTE_HIDDEN 0x2
#define FILE_ATTRIBUTE_SYSTEM 0x4
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_ARCHIVE 0x20
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_TEMPORARY 0x100
#define MOVEFILE_REPLACE_EXISTING 0x1
#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5
#define ERROR_ALREADY_EXISTS 183
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED ((DWORD)0xFFFFFFFF)
#define INFINITE 0xFFFFFFFF
#define MUTEX_ALL_ACCESS 0x1F0001
#define STD_INPUT_HANDLE ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define FORMAT_MESSAGE_FROM_SYSTEM 0x1000
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x200
#define LANG_NEUTRAL 0x00
#define LANG_USER_DEFAULT 0x400
#define SUBLANG_DEFAULT 0x01
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#define CP_ACP 0
#define CP_OEMCP 1
#define CP_UTF8 65001
#define TIME_NOSECONDS 0x2
#define TIME_NOMINUTESORSECONDS 0x1
#define KEY_READ 0x20019
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
#define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)((LONG)0x80000001))

#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CXDOUBLECLK 36
#define SM_CYDOUBLECLK 37
#define SM_CXDRAG 68
#define SM_CYDRAG 69
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9
#define SC_CLOSE 0xF060
#define MF_BYCOMMAND 0x0
#define MF_GRAYED 0x1

#define SRCCOPY 0x00CC0020
#define DIB_RGB_COLORS 0
#define BI_RGB 0
#define BI_BITFIELDS 3
#define TRANSPARENT 1
#define OPAQUE 2
#define TA_LEFT 0
#define TA_TOP 0
#define TA_CENTER 6
#define FW_NORMAL 400
#define FW_BOLD 700
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define OUT_RASTER_PRECIS 6
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define DEFAULT_PITCH 0
#define FIXED_PITCH 1
#define VARIABLE_PITCH 2
#define FF_DONTCARE 0x00
#define FF_SWISS 0x20
#define GM_ADVANCED 2
#define MWT_IDENTITY 1
#define COLORONCOLOR 3

#define MAPVK_VK_TO_VSC 0
#define MAPVK_VSC_TO_VK 1
#define MAPVK_VK_TO_CHAR 2
#define MAPVK_VSC_TO_VK_EX 3

/* Window messages, for code that still switches on them. */
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCHAR 0x0106
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MBUTTONDBLCLK 0x0209
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_USER 0x0400
#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

/* Virtual-key codes. */
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_CANCEL 0x03
#define VK_MBUTTON 0x04
#define VK_XBUTTON1 0x05
#define VK_XBUTTON2 0x06
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_CLEAR 0x0C
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SELECT 0x29
#define VK_PRINT 0x2A
#define VK_EXECUTE 0x2B
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_HELP 0x2F
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_SLEEP 0x5F
#define VK_NUMPAD0 0x60
#define VK_NUMPAD1 0x61
#define VK_NUMPAD2 0x62
#define VK_NUMPAD3 0x63
#define VK_NUMPAD4 0x64
#define VK_NUMPAD5 0x65
#define VK_NUMPAD6 0x66
#define VK_NUMPAD7 0x67
#define VK_NUMPAD8 0x68
#define VK_NUMPAD9 0x69
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SEPARATOR 0x6C
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F4 0x73
#define VK_F5 0x74
#define VK_F6 0x75
#define VK_F7 0x76
#define VK_F8 0x77
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_F13 0x7C
#define VK_F14 0x7D
#define VK_F15 0x7E
#define VK_F16 0x7F
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_VOLUME_MUTE 0xAD
#define VK_VOLUME_DOWN 0xAE
#define VK_VOLUME_UP 0xAF
#define VK_MEDIA_NEXT_TRACK 0xB0
#define VK_MEDIA_PREV_TRACK 0xB1
#define VK_MEDIA_STOP 0xB2
#define VK_MEDIA_PLAY_PAUSE 0xB3
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define VK_OEM_8 0xDF
#define VK_OEM_102 0xE2

#include "wincrt.h"
#include "io.h"
#include "malloc.h"

/* Calls. Implemented in port/emscripten/winshim.cpp. */
DWORD GetTickCount();
ULONGLONG GetTickCount64();
DWORD timeGetTime();
UINT timeBeginPeriod(UINT);
UINT timeEndPeriod(UINT);
void Sleep(DWORD ms);
DWORD GetLastError();
void SetLastError(DWORD);
BOOL QueryPerformanceCounter(LARGE_INTEGER * count);
BOOL QueryPerformanceFrequency(LARGE_INTEGER * freq);
void GetLocalTime(LPSYSTEMTIME);
void GetSystemTime(LPSYSTEMTIME);
void GetSystemTimeAsFileTime(LPFILETIME);
BOOL FileTimeToSystemTime(const FILETIME *, LPSYSTEMTIME);
BOOL SystemTimeToFileTime(const SYSTEMTIME *, LPFILETIME);
BOOL FileTimeToLocalFileTime(const FILETIME *, LPFILETIME);
LONG CompareFileTime(const FILETIME *, const FILETIME *);
int GetDateFormatA(DWORD, DWORD, const SYSTEMTIME *, LPCSTR, LPSTR, int);
int GetTimeFormatA(DWORD, DWORD, const SYSTEMTIME *, LPCSTR, LPSTR, int);
#define GetDateFormat GetDateFormatA
#define GetTimeFormat GetTimeFormatA

DWORD GetCurrentThreadId();
DWORD GetCurrentProcessId();
HANDLE GetCurrentProcess();
BOOL IsDebuggerPresent();
void OutputDebugStringA(LPCSTR);
#define OutputDebugString OutputDebugStringA
void DebugBreak();
HMODULE GetModuleHandleA(LPCSTR);
#define GetModuleHandle GetModuleHandleA
DWORD GetModuleFileNameA(HMODULE, LPSTR, DWORD);
#define GetModuleFileName GetModuleFileNameA
HMODULE LoadLibraryA(LPCSTR);
#define LoadLibrary LoadLibraryA
FARPROC GetProcAddress(HMODULE, LPCSTR);
BOOL FreeLibrary(HMODULE);
LPWSTR GetCommandLineW();
LPSTR GetCommandLineA();
#define GetCommandLine GetCommandLineA
UINT GetACP();
UINT GetOEMCP();
int MultiByteToWideChar(UINT, DWORD, LPCSTR, int, LPWSTR, int);
int WideCharToMultiByte(UINT, DWORD, LPCWSTR, int, LPSTR, int, LPCSTR, LPBOOL);
LPWSTR CharUpperW(LPWSTR);
DWORD FormatMessageA(DWORD, LPCVOID, DWORD, DWORD, LPSTR, DWORD, va_list *);
#define FormatMessage FormatMessageA
void GlobalMemoryStatus(LPMEMORYSTATUS);
UINT GetWindowsDirectoryA(LPSTR, UINT);
#define GetWindowsDirectory GetWindowsDirectoryA

BOOL CloseHandle(HANDLE);
HANDLE CreateMutexA(void *, BOOL, LPCSTR);
#define CreateMutex CreateMutexA
HANDLE OpenMutexA(DWORD, BOOL, LPCSTR);
#define OpenMutex OpenMutexA
BOOL ReleaseMutex(HANDLE);
DWORD WaitForSingleObject(HANDLE, DWORD);
void InitializeCriticalSection(LPCRITICAL_SECTION);
void DeleteCriticalSection(LPCRITICAL_SECTION);
void EnterCriticalSection(LPCRITICAL_SECTION);
void LeaveCriticalSection(LPCRITICAL_SECTION);
void InitializeSRWLock(PSRWLOCK);
void AcquireSRWLockExclusive(PSRWLOCK);
void ReleaseSRWLockExclusive(PSRWLOCK);

HANDLE CreateFileA(LPCSTR, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
#define CreateFile CreateFileA
BOOL ReadFile(HANDLE, LPVOID, DWORD, LPDWORD, void *);
BOOL WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, void *);
DWORD SetFilePointer(HANDLE, LONG, LONG *, DWORD);
DWORD GetFileSize(HANDLE, LPDWORD);
BOOL SetEndOfFile(HANDLE);
BOOL FlushFileBuffers(HANDLE);
BOOL GetFileTime(HANDLE, LPFILETIME, LPFILETIME, LPFILETIME);
BOOL SetFileTime(HANDLE, const FILETIME *, const FILETIME *, const FILETIME *);
BOOL DeleteFileA(LPCSTR);
#define DeleteFile DeleteFileA
BOOL MoveFileA(LPCSTR, LPCSTR);
#define MoveFile MoveFileA
BOOL MoveFileExA(LPCSTR, LPCSTR, DWORD);
#define MoveFileEx MoveFileExA
BOOL CopyFileA(LPCSTR, LPCSTR, BOOL);
#define CopyFile CopyFileA
BOOL CreateDirectoryA(LPCSTR, void *);
#define CreateDirectory CreateDirectoryA
BOOL RemoveDirectoryA(LPCSTR);
#define RemoveDirectory RemoveDirectoryA
DWORD GetFileAttributesA(LPCSTR);
#define GetFileAttributes GetFileAttributesA
BOOL SetFileAttributesA(LPCSTR, DWORD);
#define SetFileAttributes SetFileAttributesA
BOOL GetFileAttributesExA(LPCSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
#define GetFileAttributesEx GetFileAttributesExA
HANDLE FindFirstFileA(LPCSTR, LPWIN32_FIND_DATAA);
#define FindFirstFile FindFirstFileA
BOOL FindNextFileA(HANDLE, LPWIN32_FIND_DATAA);
#define FindNextFile FindNextFileA
BOOL FindClose(HANDLE);
DWORD GetCurrentDirectoryA(DWORD, LPSTR);
#define GetCurrentDirectory GetCurrentDirectoryA
BOOL SetCurrentDirectoryA(LPCSTR);
#define SetCurrentDirectory SetCurrentDirectoryA
DWORD GetFullPathNameA(LPCSTR, DWORD, LPSTR, LPSTR *);
#define GetFullPathName GetFullPathNameA

int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
#define MessageBox MessageBoxA
SHORT GetKeyState(int);
SHORT GetAsyncKeyState(int);
UINT MapVirtualKeyA(UINT, UINT);
#define MapVirtualKey MapVirtualKeyA
UINT MapVirtualKeyExA(UINT, UINT, HKL);
#define MapVirtualKeyEx MapVirtualKeyExA
HKL GetKeyboardLayout(DWORD);
int GetSystemMetrics(int);
UINT GetDoubleClickTime();
HWND GetCapture();
HWND GetConsoleWindow();
BOOL ShowWindow(HWND, int);
BOOL SetRect(LPRECT, int, int, int, int);
BOOL IntersectRect(LPRECT, const RECT *, const RECT *);

HDC GetDC(HWND);
int ReleaseDC(HWND, HDC);
HDC CreateCompatibleDC(HDC);
BOOL DeleteDC(HDC);
HGDIOBJ SelectObject(HDC, HGDIOBJ);
BOOL DeleteObject(HGDIOBJ);
int GetObjectA(HANDLE, int, LPVOID);
#define GetObject GetObjectA
BOOL GdiFlush();
HBITMAP CreateDIBSection(HDC, const BITMAPINFO *, UINT, void **, HANDLE, DWORD);
HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
#define CreateFont CreateFontA
HFONT CreateFontIndirectA(const LOGFONTA *);
#define CreateFontIndirect CreateFontIndirectA
BOOL GetTextMetricsA(HDC, TEXTMETRICA *);
#define GetTextMetrics GetTextMetricsA
BOOL GetTextExtentPoint32A(HDC, LPCSTR, int, LPSIZE);
#define GetTextExtentPoint32 GetTextExtentPoint32A
BOOL GetTextExtentPoint32W(HDC, LPCWSTR, int, LPSIZE);
BOOL TextOutA(HDC, int, int, LPCSTR, int);
#define TextOut TextOutA
BOOL TextOutW(HDC, int, int, LPCWSTR, int);
BOOL ExtTextOutW(HDC, int, int, UINT, const RECT *, LPCWSTR, UINT, const INT *);
COLORREF SetTextColor(HDC, COLORREF);
COLORREF SetBkColor(HDC, COLORREF);
int SetBkMode(HDC, int);
UINT SetTextAlign(HDC, UINT);
int SetGraphicsMode(HDC, int);
BOOL ModifyWorldTransform(HDC, const void *, DWORD);
int SetStretchBltMode(HDC, int);
BOOL BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD);

/* MSVC CRT pieces. */
#define _MCW_EM 0x0008001F
#define _MCW_PC 0x00030000
#define _PC_24 0x00020000
#define _PC_53 0x00010000
inline unsigned int _controlfp(unsigned int, unsigned int) { return 0; }

/* Window, console, registry and resource calls the engine reaches on paths a browser never
   takes, or takes only to be told no. */
#define WM_CANCELMODE 0x001F
#define WM_CAPTURECHANGED 0x0215
#define WM_EXITSIZEMOVE 0x0232
#define WM_EXITMENULOOP 0x0212
HWND FindWindowA(LPCSTR, LPCSTR);
#define FindWindow FindWindowA
BOOL SetForegroundWindow(HWND);
BOOL InvalidateRect(HWND, const RECT *, BOOL);
HWND SetFocus(HWND);
HWND SetCapture(HWND);
BOOL ReleaseCapture();
LRESULT DefWindowProcW(HWND, UINT, WPARAM, LPARAM);
LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
#define DefWindowProc DefWindowProcA
void InitCommonControls();
LPWSTR * CommandLineToArgvW(LPCWSTR, int *);
HANDLE LocalFree(HANDLE);
BOOL AllocConsole();
BOOL FreeConsole();
BOOL SetConsoleTitleA(LPCSTR);
#define SetConsoleTitle SetConsoleTitleA
BOOL SetConsoleOutputCP(UINT);
BOOL SetConsoleCP(UINT);
HANDLE GetStdHandle(DWORD);
BOOL GetConsoleScreenBufferInfo(HANDLE, CONSOLE_SCREEN_BUFFER_INFO *);
BOOL SetConsoleTextAttribute(HANDLE, WORD);
inline int freopen_s(FILE ** stream, const char *, const char *, FILE *) { if (stream) *stream = nullptr; return 1; }
UINT MapVirtualKeyExW(UINT, UINT, HKL);
int LoadStringA(HINSTANCE, UINT, LPSTR, int);
#define LoadString LoadStringA
DWORD GetFileVersionInfoSizeA(LPCSTR, LPDWORD);
#define GetFileVersionInfoSize GetFileVersionInfoSizeA
BOOL GetFileVersionInfoA(LPCSTR, DWORD, DWORD, LPVOID);
#define GetFileVersionInfo GetFileVersionInfoA
BOOL VerQueryValueA(LPCVOID, LPCSTR, LPVOID *, UINT *);
#define VerQueryValue VerQueryValueA
LONG RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, PHKEY);
#define RegOpenKeyEx RegOpenKeyExA
LONG RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
#define RegQueryValueEx RegQueryValueExA
LONG RegCloseKey(HKEY);
BOOL GetDiskFreeSpaceExA(LPCSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
#define GetDiskFreeSpaceEx GetDiskFreeSpaceExA
int SaveDC(HDC);
BOOL RestoreDC(HDC, int);
BOOL SetViewportOrgEx(HDC, int, int, LPPOINT);
BOOL SetWindowOrgEx(HDC, int, int, LPPOINT);
BOOL DPtoLP(HDC, LPPOINT, int);
BOOL StretchBlt(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);
BOOL SetStdHandle(DWORD, HANDLE);
BOOL SetConsoleScreenBufferSize(HANDLE, COORD);
BOOL WriteConsoleA(HANDLE, const void *, DWORD, LPDWORD, LPVOID);
#define WriteConsole WriteConsoleA
HMENU GetSystemMenu(HWND, BOOL);
BOOL DeleteMenu(HMENU, UINT, UINT);
