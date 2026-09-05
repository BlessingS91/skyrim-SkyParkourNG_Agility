#include "ParkourRequirements.h"

#include <SimpleIni.h>

namespace ParkourRequirements
{
    namespace
    {
        constexpr std::size_t RequirementCount = static_cast<std::size_t>(ParkourType::Highest) + 1;

        Requirement g_requirements[RequirementCount]{};
        Requirement g_slideRequirement{};
        Requirement g_landingRollRequirement{};

        bool g_debugLogging{false};
        RE::ActorValue g_skillType{RE::ActorValue::kSneak};

        std::size_t ToIndex(ParkourType type) { return static_cast<std::size_t>(type); }

        const char *GetTypeName(ParkourType type)
        {
            switch (type)
            {
                case ParkourType::Highest:
                    return "Highest";
                case ParkourType::High:
                    return "High";
                case ParkourType::Medium:
                    return "Medium";
                case ParkourType::Low:
                    return "Low";
                case ParkourType::StepHigh:
                    return "StepHigh";
                case ParkourType::StepLow:
                    return "StepLow";
                case ParkourType::Vault:
                    return "Vault";
                case ParkourType::Grab:
                    return "Grab";
                default:
                    return "Unknown";
            }
        }

        RE::ActorValue ParseSkillType(const char *value)
        {
            if (!value || !*value)
            {
                return RE::ActorValue::kSneak;
            }

            const std::string skill = value;

            if (skill == "OneHanded") return RE::ActorValue::kOneHanded;
            if (skill == "TwoHanded") return RE::ActorValue::kTwoHanded;
            if (skill == "Archery") return RE::ActorValue::kArchery;
            if (skill == "Block") return RE::ActorValue::kBlock;
            if (skill == "Smithing") return RE::ActorValue::kSmithing;
            if (skill == "HeavyArmor") return RE::ActorValue::kHeavyArmor;
            if (skill == "LightArmor") return RE::ActorValue::kLightArmor;
            if (skill == "Pickpocket") return RE::ActorValue::kPickpocket;
            if (skill == "Lockpicking") return RE::ActorValue::kLockpicking;
            if (skill == "Sneak") return RE::ActorValue::kSneak;
            if (skill == "Alchemy") return RE::ActorValue::kAlchemy;
            if (skill == "Speech") return RE::ActorValue::kSpeech;
            if (skill == "Alteration") return RE::ActorValue::kAlteration;
            if (skill == "Conjuration") return RE::ActorValue::kConjuration;
            if (skill == "Destruction") return RE::ActorValue::kDestruction;
            if (skill == "Illusion") return RE::ActorValue::kIllusion;
            if (skill == "Restoration") return RE::ActorValue::kRestoration;
            if (skill == "Enchanting") return RE::ActorValue::kEnchanting;

            logger::warn("ParkourRequirements: Unknown SkillType '{}', defaulting to Sneak", skill);

            return RE::ActorValue::kSneak;
        }

        void LoadRequirement(CSimpleIniA &ini, Requirement &requirement, const char *section)
        {
            logger::info("ParkourRequirements: Loading section [{}]", section);

            const double level = ini.GetDoubleValue(section, "Level", 0.0);

            requirement.level = static_cast<float>(level);

            const char *perkPlugin = ini.GetValue(section, "PerkPlugin", "");

            if (perkPlugin)
            {
                requirement.perkPlugin = perkPlugin;
            }
            else
            {
                requirement.perkPlugin.clear();
            }

            const char *perkFormID = ini.GetValue(section, "PerkFormID", "");

            requirement.perkFormID = 0;
            requirement.perk = nullptr;

            if (perkFormID && *perkFormID)
            {
                try
                {
                    requirement.perkFormID = static_cast<RE::FormID>(std::stoul(perkFormID, nullptr, 0));
                } catch (...)
                {
                    logger::error("ParkourRequirements: [{}] Invalid PerkFormID '{}'", section, perkFormID);

                    requirement.perkFormID = 0;
                }
            }

            logger::info("ParkourRequirements: [{}] Loaded | Level={:.1f} | Plugin='{}' | FormID={:08X}", section, requirement.level,
                         requirement.perkPlugin, requirement.perkFormID);
        }

        void ResolvePerk(Requirement &requirement, const char *name)
        {
            requirement.perk = nullptr;

            if (requirement.perkPlugin.empty() || requirement.perkFormID == 0)
            {
                logger::info("ParkourRequirements: [{}] No perk configured -> using configured skill level", name);

                return;
            }

            auto *dataHandler = RE::TESDataHandler::GetSingleton();

            if (!dataHandler)
            {
                logger::error("ParkourRequirements: [{}] TESDataHandler unavailable", name);

                return;
            }

            auto *form = dataHandler->LookupForm(requirement.perkFormID, requirement.perkPlugin);

            if (!form)
            {
                logger::error("ParkourRequirements: [{}] LookupForm FAILED | {} | {:08X}", name, requirement.perkPlugin,
                              requirement.perkFormID);

                return;
            }

            requirement.perk = skyrim_cast<RE::BGSPerk *>(form);

            if (!requirement.perk)
            {
                logger::error("ParkourRequirements: [{}] Form is NOT a BGSPerk", name);

                return;
            }

            logger::info("ParkourRequirements: [{}] Perk resolved | {:08X}", name, requirement.perk->GetFormID());
        }

        bool MeetsRequirementInternal(RE::Actor *actor, const Requirement &requirement, const char *name)
        {
            if (!actor)
            {
                if (g_debugLogging)
                {
                    logger::error("ParkourRequirements: MeetsRequirementInternal() received NULL actor");
                }

                return false;
            }

            if (g_debugLogging)
            {
                logger::info(
                    "ParkourRequirements: Checking [{}] | Actor={:08X} | Level={:.1f} | "
                    "PerkPlugin='{}' | PerkFormID={:08X} | PerkResolved={}",
                    name, actor->GetFormID(), requirement.level, requirement.perkPlugin, requirement.perkFormID,
                    requirement.perk != nullptr);
            }

            //
            // Perk requirement
            //

            if (!requirement.perkPlugin.empty() && requirement.perkFormID != 0)
            {
                if (!requirement.perk)
                {
                    if (g_debugLogging)
                    {
                        logger::warn("ParkourRequirements: [{}] FAIL - perk configured but not resolved", name);
                    }

                    return false;
                }

                const bool hasPerk = actor->HasPerk(requirement.perk);

                if (g_debugLogging)
                {
                    logger::info("ParkourRequirements: [{}] HasPerk={} -> {}", name, hasPerk, hasPerk ? "PASS" : "FAIL");
                }

                return hasPerk;
            }

            //
            // Skill level requirement
            //

            auto *avOwner = actor->AsActorValueOwner();

            if (!avOwner)
            {
                if (g_debugLogging)
                {
                    logger::error("ParkourRequirements: [{}] ActorValueOwner is NULL", name);
                }

                return false;
            }

            const float skill = avOwner->GetActorValue(g_skillType);

            const bool meetsLevel = skill >= requirement.level;

            if (g_debugLogging)
            {
                logger::info("ParkourRequirements: [{}] Skill={:.1f} Required={:.1f} -> {}", name, skill, requirement.level,
                             meetsLevel ? "PASS" : "FAIL");
            }

            return meetsLevel;
        }
    }

    void Load()
    {
        const std::string path = "Data/SKSE/Plugins/ParkourRequirements.ini";

        logger::info("ParkourRequirements: Loading '{}'", path);

        CSimpleIniA ini;
        ini.SetUnicode();

        const auto result = ini.LoadFile(path.c_str());

        if (result < 0)
        {
            logger::error("ParkourRequirements: FAILED to load INI '{}'", path);

            return;
        }

        // Debug setting
        g_debugLogging = ini.GetBoolValue("Debug", "EnableLogging", false);

        const char *skillType = ini.GetValue("Settings", "SkillType", "Sneak");

        g_skillType = ParseSkillType(skillType);

        logger::info("ParkourRequirements: Skill type = '{}'", skillType);

        logger::info("ParkourRequirements: Debug logging = {}", g_debugLogging ? "ON" : "OFF");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::Highest)], "ParkourRequirements.Highest");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::High)], "ParkourRequirements.High");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::Medium)], "ParkourRequirements.Medium");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::Low)], "ParkourRequirements.Low");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::StepHigh)], "ParkourRequirements.StepHigh");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::StepLow)], "ParkourRequirements.StepLow");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::Vault)], "ParkourRequirements.Vault");

        LoadRequirement(ini, g_requirements[ToIndex(ParkourType::Grab)], "ParkourRequirements.Grab");

        LoadRequirement(ini, g_slideRequirement, "ParkourRequirements.Slide");

        LoadRequirement(ini, g_landingRollRequirement, "ParkourRequirements.LandingRoll");

        ResolvePerks();

        logger::info("ParkourRequirements: Initialization complete");
    }

    void ResolvePerks()
    {
        logger::info("ParkourRequirements: Resolving perks...");

        auto *dataHandler = RE::TESDataHandler::GetSingleton();

        if (!dataHandler)
        {
            logger::error("ParkourRequirements: TESDataHandler unavailable");

            return;
        }

        for (std::size_t i = ToIndex(ParkourType::Grab); i < RequirementCount; ++i)
        {
            const auto type = static_cast<ParkourType>(i);

            ResolvePerk(g_requirements[i], GetTypeName(type));
        }

        ResolvePerk(g_slideRequirement, "Slide");

        ResolvePerk(g_landingRollRequirement, "LandingRoll");

        logger::info("ParkourRequirements: Perk resolution complete");
    }

    bool MeetsRequirement(RE::Actor *actor, ParkourType type)
    {
        const auto index = ToIndex(type);

        if (index >= RequirementCount)
        {
            if (g_debugLogging)
            {
                logger::error("ParkourRequirements: MeetsRequirement() invalid type={} index={} count={}", GetTypeName(type), index,
                              RequirementCount);
            }

            return false;
        }

        return MeetsRequirementInternal(actor, g_requirements[index], GetTypeName(type));
    }

    bool MeetsSlideRequirement(RE::Actor *actor) { return MeetsRequirementInternal(actor, g_slideRequirement, "Slide"); }

    bool MeetsLandingRollRequirement(RE::Actor *actor) { return MeetsRequirementInternal(actor, g_landingRollRequirement, "LandingRoll"); }

    const Requirement &GetRequirement(ParkourType type)
    {
        const auto index = ToIndex(type);

        if (index >= RequirementCount)
        {
            if (g_debugLogging)
            {
                logger::error("ParkourRequirements: GetRequirement() invalid type={} index={} count={}", GetTypeName(type), index,
                              RequirementCount);
            }

            static const Requirement empty{};
            return empty;
        }

        return g_requirements[index];
    }
}