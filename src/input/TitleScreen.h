// Title-screen behavior shared by the later games with a native frontend. MIT; see LICENSE.
#pragma once

const char* TitleScreenPrompt();
void SetTitleScreenText();
#ifdef GAME_MW
int TitleScreenVisible();
#endif
