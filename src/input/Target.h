// Target selection and capabilities shared by the runtime and config emitter.
#pragma once

#if defined(GAME_UG) || defined(GAME_UG2)
#define INPUT_SCANNERS 1
#else
#define INPUT_SCANNERS 0
#endif
#if defined(GAME_MW) || defined(GAME_CARBON) || defined(GAME_PROSTREET) || defined(GAME_UC)
#define HAS_RUMBLE 1
#endif
#if defined(GAME_UC) || defined(GAME_WORLD)
#define NO_FENG
#endif

namespace ConfigDefaults {
enum GameMask : unsigned {
  UG = 1, UG2 = 2, MW = 4, CB = 8, UC = 16, PS = 32, W = 64,
  All = UG | UG2 | MW | CB | UC | PS | W,
  Scanners = UG | UG2,
  Later = All & ~Scanners,
  Frontend = MW | CB | PS,
  Feedback = Scanners | Frontend | UC,
  Triggers = Feedback & ~UG,
};

#if defined(GAME_UG)
constexpr unsigned Target = UG;
constexpr const char* TargetName = "UG";
#elif defined(GAME_UG2)
constexpr unsigned Target = UG2;
constexpr const char* TargetName = "UG2";
#elif defined(GAME_MW)
constexpr unsigned Target = MW;
constexpr const char* TargetName = "MW";
#elif defined(GAME_CARBON)
constexpr unsigned Target = CB;
constexpr const char* TargetName = "CB";
#elif defined(GAME_UC)
constexpr unsigned Target = UC;
constexpr const char* TargetName = "UC";
#elif defined(GAME_PROSTREET)
constexpr unsigned Target = PS;
constexpr const char* TargetName = "PS";
#elif defined(GAME_WORLD)
constexpr unsigned Target = W;
constexpr const char* TargetName = "W";
#else
#error Select an XtendedInput game target
#endif
constexpr bool IsScanner = (Target & Scanners) != 0;
constexpr bool HasFrontend = (Target & Frontend) != 0;
constexpr const char* PluginName = IsScanner ? "NFSU_XtendedInput" : "NFS_XtendedInput";
}
