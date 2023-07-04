#pragma once

#include "GlobalNamespace/IReadonlyBeatmapData.hpp"

bool SettingsAreDefault(bool for90Degree);

GlobalNamespace::IReadonlyBeatmapData* Generate(GlobalNamespace::IReadonlyBeatmapData* base, float bpm, bool is90Degree, bool leftHanded);
