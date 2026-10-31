@echo off
set "GAME="
set "NAME=NFS_XtendedInput"
if /I "%~1"=="UG" set "GAME=GAME_UG"
if /I "%~1"=="UG2" set "GAME=GAME_UG2"
if /I "%~1"=="MW" set "GAME=GAME_MW"
if /I "%~1"=="CB" set "GAME=GAME_CARBON"
if /I "%~1"=="UC" set "GAME=GAME_UC"
if /I "%~1"=="PS" set "GAME=GAME_PROSTREET"
if /I "%~1"=="W" set "GAME=GAME_WORLD"
if not defined GAME (
  echo Expected UG, UG2, MW, CB, UC, PS or W.
  exit /b 1
)
if /I "%~1"=="UG" set "NAME=NFSU_XtendedInput"
if /I "%~1"=="UG2" set "NAME=NFSU_XtendedInput"
exit /b 0
