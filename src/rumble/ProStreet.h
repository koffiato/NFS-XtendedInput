// ProStreet PC feedback bindings, checked against the user's running game.
// No console executable or console curve data is included.
#pragma once

// The native GameDevice has a 21-slot IFeedback at +0x2C. Its road callback is
// slot 18, not MW's slot 6; slot 19 also passes a velocity vector by value.
#ifndef RUMBLE_PS_IMAGE_BASE
#define RUMBLE_PS_IMAGE_BASE 0x400000u
#endif
uintptr_t RumblePSAddress(uintptr_t address) { return RUMBLE_PS_IMAGE_BASE + address - 0x400000u; }

struct RumblePSVector { float x, y, z; };
static_assert(sizeof(RumblePSVector) == 12, "PS collision argument size");
struct COLLISION_INFO;
void __fastcall RumblePSCollisionHook(void*, void*, const COLLISION_INFO&);

bool RumbleValidateProStreetUnsafe() {
  const unsigned int feedback[] = {
      0x6C7F60, 0x6A5A40, 0x402CC0, 0x890020, 0x402CC0, 0x402CC0,
      0x44AC10, 0x82D870, 0x44AC10, 0x44AC10, 0x7C71A0, 0x7B55E0,
      0x7C71A0, 0x7C71A0, 0x7C71A0, 0x44AC10, 0x87E0A0, 0x7C71A0,
      0x44AC10, 0x82D870, 0x6AD170};
  if (memcmp((const void*)RumblePSAddress(0x98BE6C), feedback, sizeof(feedback))) return false;
  const unsigned char getter[] = {0x85,0xC9,0x74,0x04,0x8D,0x41,0x2C,0xC3};
  const unsigned char attrib[] = {0x8B,0x09,0x85,0xC9,0x74,0x05,0xE9};
  const unsigned char find[] = {0x8B,0x11,0x56,0x8B,0x71,0x04};
  const unsigned char registration[] = {0x68,0xD0,0xD5,0x4C,0x00,0x8B,0xCD};
  const unsigned char collisionGate[] = {0x80,0x78,0x1D,0x00,0x0F,0x84,0xB6,0x00,0x00,0x00};
  const unsigned char skidCall[] = {0x83,0xC0,0x1C};
  const unsigned char slipCall[] = {0x83,0xC0,0x20};
  const unsigned char roadCall[] = {0x83,0xC0,0x48};
  const unsigned char collisionCall[] = {0x8B,0x53,0x4C};
  return *(unsigned int*)RumblePSAddress(0x98BE38) == 0x6C7F20 && // InputDevice slot 9
      *(unsigned int*)RumblePSAddress(0x9FA07C) == 0x4D13F0 &&
      *(unsigned int*)RumblePSAddress(0x9FA324) == 0x4D13F0 &&
      !memcmp((void*)RumblePSAddress(0x6C7F20), getter, sizeof(getter)) &&
      !memcmp((void*)RumblePSAddress(ATTRIB_GETATTRIBPTR_ADDR), attrib, sizeof(attrib)) &&
      !memcmp((void*)RumblePSAddress(UTL_ILIST_FIND_ADDR), find, sizeof(find)) &&
      !memcmp((void*)RumblePSAddress(0x6C7DE9), registration, sizeof(registration)) &&
      !memcmp((void*)RumblePSAddress(0x4D145A), collisionGate, sizeof(collisionGate)) &&
      !memcmp((void*)RumblePSAddress(0x4D10A3), skidCall, sizeof(skidCall)) &&
      !memcmp((void*)RumblePSAddress(0x4D1063), slipCall, sizeof(slipCall)) &&
      !memcmp((void*)RumblePSAddress(0x4D103E), roadCall, sizeof(roadCall)) &&
      !memcmp((void*)RumblePSAddress(0x4D150D), collisionCall, sizeof(collisionCall));
}

bool RumbleValidateProStreet() {
  __try { return RumbleValidateProStreetUnsafe(); }
  __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
