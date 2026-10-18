#pragma once
#include "windows.h"
inline int _getch() { return 0; }
#define getch _getch
inline int _kbhit() { return 0; }
#define kbhit _kbhit
