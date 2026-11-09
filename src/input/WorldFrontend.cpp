// World native device-scalar compatibility hooks. MIT; see LICENSE.
#include "Target.h"
#ifdef GAME_WORLD
#include "../_vs/stdafx.h"
#include "Addresses.h"
#include "WorldFrontend.h"

// world, for whatever reason, uses direct pointers to the class instead of accessing the device scalar array normally
// copied this code snippet from Carbon 0x00696166
static unsigned int loc_75E375;
static unsigned int loc_75E363;
void __declspec(naked) UnscrewWorldDeviceScalars() {
  _asm
  {
        mov ecx, [ebx+0x18]
        shl esi, 4
        add esi, ecx
        test esi, esi
        jnz unscrew_jnz
        jmp loc_75E363
        unscrew_jnz:
        jmp loc_75E375
  }
}

static unsigned int loc_75E237;
static unsigned int loc_75E225;
void __declspec(naked) UnscrewWorldDeviceScalars2() {
  _asm
  {
        mov ecx, [ebx + 0x18]
        shl edi, 4
        add edi, ecx
        mov [ebp-8], edi
        test edi, edi
        jnz unscrew2_jnz
        jmp loc_75E225
        unscrew2_jnz :
        jmp loc_75E237
  }
}

static unsigned int loc_75E25F;
void __declspec(naked) UnscrewWorldDeviceScalars3() {
  _asm
  {
        mov edx, [ebx + 0x18]
        shl edi, 4
        add edi, edx
        jmp loc_75E25F
  }
}
void InitializeWorldDeviceScalars() {
  loc_75E375 = 0x75E375 + MainBase;
  loc_75E363 = 0x75E363 + MainBase;
  loc_75E237 = 0x75E237 + MainBase;
  loc_75E225 = 0x75E225 + MainBase;
  loc_75E25F = 0x75E25F + MainBase;
}
#endif
