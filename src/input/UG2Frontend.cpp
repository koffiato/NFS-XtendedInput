// UG2 native frontend implementation. MIT; see LICENSE.
#include "Target.h"
#ifdef GAME_UG2
#include "../_vs/stdafx.h"
#include "State.h"
#include "Addresses.h"
#include "UG2Frontend.h"

#include <windows.h>
#include <timeapi.h>
#pragma comment(lib,"winmm.lib")

void(*FESendKeystroke)(unsigned int key) = (void(*)(unsigned int))FE_SENDKEY_ADDR;

void UpdateFECursorPos()
{
	bool bMouseInGameWindow = false;
	POINT MousePos;
	GetCursorPos(&MousePos);
	GetWindowRect(*(HWND*)GAME_HWND_ADDR, &windowRect);

	float ratio = 480.0 / (windowRect.bottom - windowRect.top); // scaling it to 480 height since that's what FE wants

	if (MousePos.x >= windowRect.left && MousePos.x <= windowRect.right &&
	    MousePos.y >= windowRect.top && MousePos.y <= windowRect.bottom)
		bMouseInGameWindow = true;

	MousePos.x = MousePos.x - windowRect.left;
	MousePos.y = MousePos.y - windowRect.top;

	MousePos.x = (int)((float)(MousePos.x) * ratio);
	MousePos.y = (int)((float)(MousePos.y) * ratio);


	static int LastPhysicalMousePosX = -1;
	static int LastPhysicalMousePosY = -1;
	static int LastOrbitMouseX = 0;
	static int LastOrbitMouseY = 0;
	static bool bCursorHiddenOld = false;

	if (!bMouseInGameWindow)
	{
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		bCursorHidden = true;
	}
	else if ((MousePos.x != LastPhysicalMousePosX) || (MousePos.y != LastPhysicalMousePosY))
	{
		TimeSinceLastMouseMovement = timeGetTime();
		LastPhysicalMousePosX = MousePos.x;
		LastPhysicalMousePosY = MousePos.y;
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		bCursorHidden = false;
	}
	else
	{
		if ((TimeSinceLastMouseMovement + (DWORD)MOUSEHIDE_TIME) < timeGetTime())
		{
			SetCursor(NULL);
			bCursorHidden = true;
		}
	}

	if (bCursorHidden)
	{
		*(int*)FEMOUSECURSOR_CARORBIT_X_ADDR = 0;
		*(int*)FEMOUSECURSOR_CARORBIT_Y_ADDR = 0;
		*(int*)FEMOUSECURSOR_X_ADDR = -10000;
		*(int*)FEMOUSECURSOR_Y_ADDR = -10000;
		*(bool*)FEMOUSECURSOR_BUTTONPRESS_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTONPRESS2_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTONPRESS3_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS2_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON2PRESS3_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS2_ADDR = false;
		*(bool*)FEMOUSECURSOR_BUTTON3PRESS3_ADDR = false;
		bMousePressedDown = false;
		bMouse2PressedDown = false;
		bMouse3PressedDown = false;
		bMousePressedDownOldState = false;
		bMouse2PressedDownOldState = false;
		bMouse3PressedDownOldState = false;
		bCursorHiddenOld = true;
	}
	else
	{
		// car orbiting position calculation - always relative to old
		if (bCursorHiddenOld) {
			*(int*)FEMOUSECURSOR_CARORBIT_X_ADDR = 0;
			*(int*)FEMOUSECURSOR_CARORBIT_Y_ADDR = 0;
		} else {
			*(int*)FEMOUSECURSOR_CARORBIT_X_ADDR = MousePos.x - LastOrbitMouseX;
			*(int*)FEMOUSECURSOR_CARORBIT_Y_ADDR = MousePos.y - LastOrbitMouseY;
		}
		LastOrbitMouseX = MousePos.x;
		LastOrbitMouseY = MousePos.y;

		*(int*)FEMOUSECURSOR_X_ADDR = MousePos.x;
		*(int*)FEMOUSECURSOR_Y_ADDR = MousePos.y;
		bCursorHiddenOld = false;

		// track mouse click state - make sure it lasts for exactly 1 tick of a loop, because the game is a little overzelaous with reading this input
		if (bMousePressedDown != bMousePressedDownOldState)
		{
			*(bool*)FEMOUSECURSOR_BUTTONPRESS_ADDR = bMousePressedDown;
			*(bool*)FEMOUSECURSOR_BUTTONPRESS2_ADDR = bMousePressedDown;
			*(bool*)FEMOUSECURSOR_BUTTONPRESS3_ADDR = !bMousePressedDown;
			bMousePressedDownOldState = bMousePressedDown;
		}
		else
		{
			//*(bool*)FEMOUSECURSOR_BUTTONPRESS_ADDR = false; // except this one, it's used for car orbiting
			*(bool*)FEMOUSECURSOR_BUTTONPRESS2_ADDR = false;
			*(bool*)FEMOUSECURSOR_BUTTONPRESS3_ADDR = false;
		}

		// track mouse click state - make sure it lasts for exactly 1 tick of a loop, because the game is a little overzelaous with reading this input
		if (bMouse2PressedDown != bMouse2PressedDownOldState)
		{
			*(bool*)FEMOUSECURSOR_BUTTON2PRESS_ADDR = bMouse2PressedDown;
			*(bool*)FEMOUSECURSOR_BUTTON2PRESS2_ADDR = bMouse2PressedDown;
			*(bool*)FEMOUSECURSOR_BUTTON2PRESS3_ADDR = !bMouse2PressedDown;
			bMouse2PressedDownOldState = bMouse2PressedDown;
		}
		else
		{
			*(bool*)FEMOUSECURSOR_BUTTON2PRESS2_ADDR = false;
			*(bool*)FEMOUSECURSOR_BUTTON2PRESS3_ADDR = false;
		}
		// track mouse click state - make sure it lasts for exactly 1 tick of a loop, because the game is a little overzelaous with reading this input
		if (bMouse3PressedDown != bMouse3PressedDownOldState)
		{
			*(bool*)FEMOUSECURSOR_BUTTON3PRESS_ADDR = bMouse3PressedDown;
			*(bool*)FEMOUSECURSOR_BUTTON3PRESS2_ADDR = bMouse3PressedDown;
			*(bool*)FEMOUSECURSOR_BUTTON3PRESS3_ADDR = !bMouse3PressedDown;
			bMouse3PressedDownOldState = bMouse3PressedDown;
		}
		else
		{		
			*(bool*)FEMOUSECURSOR_BUTTON3PRESS2_ADDR = false;
			*(bool*)FEMOUSECURSOR_BUTTON3PRESS3_ADDR = false;
		}
	}
}
#endif
