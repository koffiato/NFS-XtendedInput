// Native config emitter. Shares defaults and event tables with the ASI.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <XInput.h>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../src/input/ConfigDefaults.h"
#include "../src/input/Bindings.h"
#include "../src/input/Keyboard.h"
#if defined(GAME_UG)
#include "../src/input/UGEvents.h"
#elif defined(GAME_UG2)
#include "../src/input/UG2Events.h"
#else
#include "../src/input/Actions.h"
#endif
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;
using namespace ConfigDefaults;

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) throw std::runtime_error("Cannot read " + path.u8string());
  return std::string(std::istreambuf_iterator<char>(file), {});
}

std::string LF(const std::string& text) {
  std::string result;
  for (char ch : text) if (ch != '\r') result += ch;
  return result;
}

std::string CRLF(const std::string& text) {
  std::string result;
  for (char ch : text) { if (ch == '\n') result += '\r'; result += ch; }
  return result;
}

void Write(const fs::path& path, const std::string& text) {
  std::string bytes = CRLF(text);
  if (fs::exists(path) && Read(path) == bytes) return;
  std::ofstream file(path, std::ios::binary);
  if (!file.write(bytes.data(), bytes.size())) throw std::runtime_error("Cannot write " + path.u8string());
}

std::string Comment(const std::string& text) {
  std::istringstream words(text);
  std::string word, line, result;
  while (words >> word) {
    if (!line.empty() && line.size() + word.size() + 1 > 84) {
      result += "; " + line + "\n";
      line.clear();
    }
    if (!line.empty()) line += ' ';
    line += word;
  }
  return result + (line.empty() ? "" : "; " + line + "\n");
}

std::string Text(bool value) { return value ? "1" : "0"; }
std::string Text(int value) { return std::to_string(value); }
std::string Text(const char* value) { return value; }
std::string Text(float value) {
  char buffer[64];
  auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
  if (converted.ec != std::errc()) throw std::runtime_error("Cannot format a configuration default");
  std::string result(buffer, converted.ptr);
  if (result.find_first_of(".eE") == std::string::npos) result += ".0";
  if (value > -1 && value < 1 && result.find_first_of("eE") == std::string::npos &&
      result.size() - result.find('.') == 2) result += '0';
  return result;
}

std::string Settings() {
  const char* name = Target == CB ? "Carbon" : Target == PS ? "ProStreet" : Target == W ? "World" : TargetName;
  std::string result = Comment(std::string("XtendedInput settings for ") + name +
      ". Existing INIs are preserved; edit this file to customize your controls.");
  std::string section, note;
  bool emitted = false;
  auto setting = [&](const char* key, const std::string& value, const char* description) {
    if (!emitted) {
      result += "\n[" + section + "]\n" + Comment(note);
      emitted = true;
    }
    result += Comment(description);
    result += (IsScanner && section == "Deadzone" ? "Deadzone" : "") + std::string(key) + " = " + value + "\n";
  };
#define CONFIG_BEGIN(section)
#define CONFIG_END()
#define CONFIG_SECTION(name, description) section = #name; note = description; emitted = false;
#define CONFIG_SETTING(scope, section, key, type, value, description) \
  if ((scope) & Target) setting(#key, Text(ConfigDefaults::section::key), description);
#include "../config/settings.def"
#undef CONFIG_SETTING
#undef CONFIG_SECTION
#undef CONFIG_BEGIN
#undef CONFIG_END
  return result;
}

std::vector<std::string> Events() {
  std::vector<std::string> result;
#if INPUT_SCANNERS
  for (int i = 0; i < (int)_countof(JoyEventNames); ++i) {
    std::string name = JoyEventNames[i];
#else
  for (int i = 0; i < MAX_ACTIONID; ++i) {
    std::string name = ActionIDStr[i];
#endif
    if (name.empty() || name == "NULL_ACTION" || name == "JOY_EVENT_NULL" || name == "MAX_ACTIONID" || name.find("Unknown_") == 0) continue;
    if (std::find(result.begin(), result.end(), name) == result.end()) result.push_back(name);
  }
  return result;
}

std::vector<std::string> ExtraKeys() {
  if (IsScanner) return {"KeyboardSteerLeft", "KeyboardSteerRight", "SteerLeftAxis", "SteerLeftButton", "SteerRightAxis", "SteerRightButton"};
  return {"FRONTENDACTION_DOWN_ALT", "FRONTENDACTION_LEFT_ALT", "FRONTENDACTION_RIGHT_ALT", "FRONTENDACTION_UP_ALT"};
}

struct Binding { std::string section, event, value; };

std::vector<Binding> Bindings(const fs::path& root) {
  std::vector<Binding> result;
  auto events = Events();
  auto extra = ExtraKeys();
  std::set<std::string> supported(events.begin(), events.end());
  supported.insert(extra.begin(), extra.end());
  std::set<std::string> targets = {"UG", "UG2", "MW", "CB", "UC", "PS", "W"};
  std::set<std::string> sections = {"Events", "Events_Secondary", "EventsKB", "EventsKB_Secondary"};
  std::set<std::pair<std::string, std::string>> seen;
  std::istringstream input(LF(Read(root / "config/bindings.csv")));
  std::string line;
  std::getline(input, line);
  if (line != "games,section,event,binding") throw std::runtime_error("Invalid binding column header");
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    std::istringstream row(line);
    std::string games, section, event, value, surplus;
    if (!std::getline(row, games, ',') || !std::getline(row, section, ',') ||
        !std::getline(row, event, ',') || !std::getline(row, value, ',') || std::getline(row, surplus, ','))
      throw std::runtime_error("Expected four binding columns: " + line);
    std::istringstream gameNames(games);
    std::string game;
    bool selected = false;
    while (gameNames >> game) {
      if (!targets.count(game)) throw std::runtime_error("Unknown binding target: " + game);
      if (game == TargetName) selected = true;
    }
    if (!selected) continue;
    if (!sections.count(section) || !supported.count(event)) throw std::runtime_error("Unsupported binding: " + line);
    if (!seen.insert({section, event}).second) throw std::runtime_error("Duplicate binding: " + line);
    bool keyboard = section.find("KB") != std::string::npos;
    bool valid = keyboard
        ? (value.size() == 1 && std::isalnum((unsigned char)value[0])) || InputVirtualKey(value.c_str(), !IsScanner)
        : value == "0" || value == "-1" || InputBindings::ParseButton(value.c_str(), !IsScanner) ||
          InputBindings::ParseAnalog(value.c_str(), !IsScanner, IsScanner);
    if (!valid) throw std::runtime_error("Invalid binding value: " + line);
    result.push_back({section, event, value});
  }
  return result;
}

std::string Group(const std::string& event) {
  if (event.find("DEBUG_CAMERA") != std::string::npos || event.find("DEBUGACTION_") == 0 || event == "CAMERAACTION_DEBUG") return "Debug camera";
  for (const char* prefix : {"FRONTENDACTION_", "JOY_EVENT_FENG_", "JOY_EVENT_MENU_", "JOY_EVENT_CARSEL_"})
    if (event.find(prefix) == 0) return "Menus and frontend";
  if (event == "JOY_EVENT_START" || event == "JOY_EVENT_SELECT" || event == "JOY_EVENT_SKIP") return "Menus and frontend";
  return "Driving";
}

std::string BindingINI(const std::vector<Binding>& bindings) {
  std::string result = Comment("One control per event. Use XINPUT_GAMEPAD names for gamepad bindings, and VK names or single characters for keyboard bindings. Put comments on their own lines; inline comments are not supported on binding values.");
  for (const char* section : {"Events", "Events_Secondary", "EventsKB", "EventsKB_Secondary"}) {
    bool emitted = false;
    for (const char* group : {"Menus and frontend", "Driving", "Debug camera"}) {
      bool groupEmitted = false;
      for (const auto& binding : bindings) {
        if (binding.section != section || Group(binding.event) != group) continue;
        if (!emitted) { result += std::string("\n[") + section + "]\n"; emitted = true; }
        if (!groupEmitted) { result += Comment(group); groupEmitted = true; }
        result += binding.event + " = " + binding.value + "\n";
      }
    }
  }
  return result;
}

std::string Reference() {
  std::string result = std::string("XtendedInput event reference: ") + TargetName + "\n\n";
  result += Comment("These names come from the runtime's event table. Availability of an event does not establish that the PC game produces it. Use [Events] for gamepad input and [EventsKB] for keyboard input.");
  result += "\nAvailable gamepad bindings:\n";
  for (const auto& binding : InputBindings::bindings) result += std::string(binding.name) + "\n";
  result += "XINPUT_GAMEPAD_LB\nXINPUT_GAMEPAD_RB\n";
  if (IsScanner) result += "XINPUT_GAMEPAD_DPAD\n";
  result += "\nKeyboard bindings use Windows VK_* names or single letters/digits.\nDo not put inline comments after binding values.\n\nAvailable events:\n";
  for (const auto& event : Events()) result += event + "\n";
  result += "\nAdditional mapping keys:\n";
  for (const auto& key : ExtraKeys()) result += key + "\n";
  return result;
}

std::string SHA256(const std::string& data) {
  HCRYPTPROV provider = 0;
  HCRYPTHASH hash = 0;
  BYTE bytes[32];
  DWORD size = sizeof(bytes);
  bool ok = CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
      CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) &&
      CryptHashData(hash, (const BYTE*)data.data(), (DWORD)data.size(), 0) &&
      CryptGetHashParam(hash, HP_HASHVAL, bytes, &size, 0);
  if (hash) CryptDestroyHash(hash);
  if (provider) CryptReleaseContext(provider, 0);
  if (!ok) throw std::runtime_error("Cannot hash configuration inputs");
  std::ostringstream result;
  for (BYTE value : bytes) result << std::hex << std::setw(2) << std::setfill('0') << (unsigned)value;
  return result.str();
}

std::string Manifest(const fs::path& root) {
  std::istringstream inputs(LF(Read(root / "config/inputs.txt")));
  std::string path, result = std::string("{\n  \"target\": \"") + TargetName + "\",\n  \"inputs\": {";
  bool first = true;
  std::set<std::string> seen;
  while (std::getline(inputs, path)) {
    if (path.empty()) continue;
    if (!seen.insert(path).second) throw std::runtime_error("Duplicate configuration input: " + path);
    if (fs::path(path).is_absolute() || path.find("..") != std::string::npos || path.find_first_of("\\\"") != std::string::npos)
      throw std::runtime_error("Expected a relative source path: " + path);
    result += (first ? "\n" : ",\n") + std::string("    \"") + path + "\": \"" + SHA256(LF(Read(root / path))) + "\"";
    first = false;
  }
  return result + "\n  }\n}\n";
}

int wmain(int argc, wchar_t** argv) {
  try {
    if (argc != 3) throw std::runtime_error("Usage: configgen <source root> <build output directory>");
    fs::path root = fs::absolute(argv[1]), output = fs::absolute(argv[2]);
    fs::create_directories(output);
    std::string settings = Settings(), bindings = BindingINI(Bindings(root));
    if (IsScanner) settings += "\n" + bindings;
    std::map<std::string, std::string> files = {
        {std::string(PluginName) + ".ini", settings}, {"EventReference.txt", Reference()}, {"ConfigManifest.json", Manifest(root)}};
    if (!IsScanner) files[std::string(PluginName) + ".default.ini"] = bindings;
    for (const char* plugin : {"NFSU_XtendedInput", "NFS_XtendedInput"})
      for (const char* suffix : {".ini", ".default.ini"}) {
        std::string name = std::string(plugin) + suffix;
        if (!files.count(name)) fs::remove(output / name);
      }
    std::string resources = "#pragma code_page(65001)\n";
    for (const auto& file : files) {
      Write(output / file.first, file.second);
      int id = file.first == "EventReference.txt" ? 103 : file.first == "ConfigManifest.json" ? 104 :
          file.first.find(".default.ini") != std::string::npos ? 102 : 101;
      resources += std::to_string(id) + " RCDATA \"" + (output / file.first).generic_u8string() + "\"\n";
    }
    Write(output / "DefaultConfig.rc", resources);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Config generation failed: " << error.what() << '\n';
    return 1;
  }
}
