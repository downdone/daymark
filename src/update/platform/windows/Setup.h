#pragma once
#include "Engine.h"
namespace daymark::update::win {
std::string setupDiagnosticCode(std::string_view key);
class SetupManager final {
  Settings settings;

public:
  explicit SetupManager(Settings value) : settings(std::move(value)) {
    settings.installationOperation = true;
  }
  Json install(const fs::path &source, const fs::path &target);
  Json remove(const fs::path &target);
  Json finish(const fs::path &target);
  Json sealUninstaller(const fs::path &target);
  Json verifyBundle(const fs::path &source);
};
} // namespace daymark::update::win
