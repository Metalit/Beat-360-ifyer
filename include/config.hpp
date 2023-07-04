#pragma once

#include "config-utils/shared/config-utils.hpp"

DECLARE_CONFIG(Config,
    CONFIG_VALUE(Show90, bool, "Show 90 Degree", false, "Shows generated 90 degree levels");
    CONFIG_VALUE(Show360, bool, "Show 360 Degree", true, "Shows generated 360 degree levels");
    CONFIG_VALUE(BasedOn, std::string, "Base Characteristic", "Standard", "Characteristic used as a base for the generated levels");

    CONFIG_VALUE(PreferredBarDuration, float, "Preferred Bar Duration", 1.84)
    CONFIG_VALUE(LimitRotations360, int, "Rotation Limit (360 Degree)", 28, "The amount of rotations before stopping rotation events (24 is one full rotation)")
    CONFIG_VALUE(BottleneckRotations360, int, "Sequential Rotations (360 Degree)", 14, "The amount of rotations before preferring the other direction (24 is one full rotation)")
    CONFIG_VALUE(LimitRotations90, int, "Rotation Limit (90 Degree)", 2, "The amount of rotations before stopping rotation events (24 is one full rotation)")
    CONFIG_VALUE(BottleneckRotations90, int, "Sequential Rotations (90 Degree)", 1, "The amount of rotations before preferring the other direction (24 is one full rotation)")
    CONFIG_VALUE(EnableSpin, bool, "Enable Spin", false, "Enable the spin effect when no notes are coming")
    CONFIG_VALUE(TotalSpinTime, float, "Spin Duration", 0.6, "The total time 1 spin takes in seconds")
    CONFIG_VALUE(SpinCooldown, float, "Spin Cooldown", 10, "Minimum amount of seconds between each spin effect")
    CONFIG_VALUE(WallFrontCut, float, "Wall Front Cut", 0.2, "Amount of time in seconds to cut of the front of a wall when rotating towards it")
    CONFIG_VALUE(WallBackCut, float, "Wall Back Cut", 0.45, "Amount of time in seconds to cut of the back of a wall when rotating towards it")
    CONFIG_VALUE(MinWallDuration, float, "Min Wall Duration", 0.1, "The minimum duration of a wall for it to be included")
    CONFIG_VALUE(WallGenerator, bool, "Generate Walls", false, "Generates extra walls, walls are cool in 360 mode")
    CONFIG_VALUE(OnlyOneSaber, bool, "One Saber", false, "Only keeps notes of one color")
)

#include "UnityEngine/GameObject.hpp"

void GameplaySetup(UnityEngine::GameObject* self, bool firstActivation);
