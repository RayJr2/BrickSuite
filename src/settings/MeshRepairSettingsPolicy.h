#pragma once

namespace MeshRepairSettingsPolicy {
constexpr bool allowsAutomaticPreparation(bool enabled) { return enabled; }
constexpr bool allowsExplicitPreparation(bool) { return true; }
} // namespace MeshRepairSettingsPolicy
