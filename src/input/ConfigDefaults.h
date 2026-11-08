// Defaults are declared once, alongside their applicability and INI comments.
#pragma once
#include "Target.h"

namespace ConfigDefaults {
#define CONFIG_SECTION(section, note)
#define CONFIG_SETTING(scope, section, key, type, value, comment) \
  constexpr type key = value;
#define CONFIG_BEGIN(section) namespace section {
#define CONFIG_END() }
#include "../../config/settings.def"
#undef CONFIG_END
#undef CONFIG_BEGIN
#undef CONFIG_SETTING
#undef CONFIG_SECTION
}
