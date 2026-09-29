#pragma once
#include "types.h"
namespace Jiit {
QString configPath();
Settings loadSettings();
bool saveSettings(const Settings &settings, QString *error = nullptr);
} // namespace Jiit
