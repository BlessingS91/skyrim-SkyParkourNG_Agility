#pragma once

#include "RE/Skyrim.h"
#include "_References/ParkourType.h"

namespace ParkourRequirements
{
    struct Requirement {
            float level{0.0f};

            std::string perkPlugin;
            RE::FormID perkFormID{0};

            RE::BGSPerk *perk{nullptr};
    };

    bool MeetsSlideRequirement(RE::Actor *actor);
    bool MeetsLandingRollRequirement(RE::Actor *actor);

    void Load();
    void ResolvePerks();

    bool MeetsRequirement(RE::Actor *actor, ParkourType type);

    const Requirement &GetRequirement(ParkourType type);
}