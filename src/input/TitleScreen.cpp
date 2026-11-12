// Native title-screen prompts and package selection. MIT; see LICENSE.
#include "Target.h"
#if defined(GAME_MW) || defined(GAME_CARBON) || defined(GAME_PROSTREET)
#include "../_vs/stdafx.h"
#include "State.h"
#include "Addresses.h"
#include "NativeFrontendAPI.h"
#include "TitleScreen.h"

const char* TitleScreenPrompt() {
  if (!bConsoleFEng || CurrentIconMode == ICON_MODE_PLAINTEXT) return FE_SPLASH_TEXT_PC;
  if (CurrentIconMode == ICON_MODE_BUILTIN) {
#if defined(GAME_MW) || defined(GAME_CARBON)
    return ControllerIconMode == CONTROLLERICON_PS4 ? FE_SPLASH_TEXT_PS4 : FE_SPLASH_TEXT_PS3;
#else
    return FE_SPLASH_TEXT_XBOX;
#endif
  }
  switch (ControllerIconMode) {
    case CONTROLLERICON_PS3: return FE_SPLASH_TEXT_PS3;
    case CONTROLLERICON_XB360MW:
    case CONTROLLERICON_XBOX360: return FE_SPLASH_TEXT_XBOX360;
    case CONTROLLERICON_PS4: return FE_SPLASH_TEXT_PS4;
    case CONTROLLERICON_SWITCH: return FE_SPLASH_TEXT_SWITCH;
    default: return FE_SPLASH_TEXT_XBOX;
  }
}

#ifdef GAME_MW
int TitleScreenVisible() {
  if (*(int*)GAMEFLOWMANAGER_STATUS_ADDR == 3) {
    if (cFEng_IsPackageInControl_Fast(SPLASH_FNG_HASH)) return 1;
    if (cFEng_IsPackageInControl_Fast(WS_SPLASH_FNG_HASH)) return 2;
  }
  return 0;
}

void SetTitleScreenText() {
  int screen = TitleScreenVisible();
  if (!screen) return;
  char* package = screen == 2 ? WSSplashPkgName : SplashPkgName;
  int object = screen == 2 ? 0xC4DF3FF2 : 0xC6CC55A4;
  FEPrintf(package, FEngFindObject_Title(package, object), TitleScreenPrompt());
}
#else
void SetTitleScreenText() {
  FE_String_Printf(FEngFindObject_Title(SplashPkgName, 0xC4DF3FF2), TitleScreenPrompt());
}
#endif
#endif
