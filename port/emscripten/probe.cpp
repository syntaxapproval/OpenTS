/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

/*
 * Readings for a test page, which polls them while the game runs.
 */

#include "always.h"

#include "_timer.h"
#include "_xmouse.h"
#include "globals.h"
#include "goptions.h"
#include "session.h"
#include "video.h"
#include "sdl/sdlwindow.h"

#include <emscripten.h>

extern "C" EMSCRIPTEN_KEEPALIVE double OpenTS_Probe_Stat(int which)
{
	switch (which) {
		case 0:		return((double)Frame);
		case 1:		return((double)Video_Present_Count());
		case 2:		return((double)Video_Frame_Upload_Count());
		case 3:		return((double)Session.Type);
		case 4:		return((double)Session.Play);
		case 5:		return((double)Options.GameSpeed);
		case 6:		return((double)(int)FrameTimer);
		case 7:		return(MouseCursor != nullptr ? (double)Get_Mouse_X() : -1.0);
		case 8:		return(MouseCursor != nullptr ? (double)Get_Mouse_Y() : -1.0);
		case 9:		{ int x = -1, y = -1; Main_Window_Cursor_Position(x, y); return((double)(x * 10000 + y)); }
		default:	return(0.0);
	}
}
