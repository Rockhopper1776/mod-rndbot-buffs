/*
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 */

#include "Config.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"
#include "Playerbots.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <algorithm>
#include <initializer_list>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
struct RndbotBuffConfig
{
    bool enabled = true;
    uint32 checkIntervalMs = 5000;
    float radius = 30.0f;
    float minimumManaPct = 35.0f;
    bool sameFactionOnly = true;
    bool skipPlayersInCombat = true;
    bool skipBattlegrounds = true;
    bool enablePriestBuffs = true;
    bool enableMageBuffs = true;
    bool enableDruidBuffs = true;
    bool enablePaladinBuffs = true;
    bool debug = false;
};

RndbotBuffConfig config;
std::unordered_map<ObjectGuid::LowType, uint32> botCheckTimers;
std::mutex botCheckTimers_mutex;

void LoadConfig()
{
    config.enabled = sConfigMgr->GetOption<bool>("RndbotBuffs.Enable", true);
    config.checkIntervalMs = std::max<uint32>(
        1000, sConfigMgr->GetOption<uint32>("RndbotBuffs.CheckIntervalMs", 5000));
    config.radius = std::clamp(
        sConfigMgr->GetOption<float>("RndbotBuffs.Radius", 30.0f), 5.0f, 100.0f);
    config.minimumManaPct = std::clamp(
        sConfigMgr->GetOption<float>("RndbotBuffs.MinimumManaPct", 35.0f), 0.0f, 100.0f);
    config.sameFactionOnly = sConfigMgr->GetOption<bool>("RndbotBuffs.SameFactionOnly", true);
    config.skipPlayersInCombat = sConfigMgr->GetOption<bool>("RndbotBuffs.SkipPlayersInCombat", true);
    config.skipBattlegrounds = sConfigMgr->GetOption<bool>("RndbotBuffs.SkipBattlegrounds", true);
    config.enablePriestBuffs = sConfigMgr->GetOption<bool>("RndbotBuffs.Classes.Priest", true);
    config.enableMageBuffs = sConfigMgr->GetOption<bool>("RndbotBuffs.Classes.Mage", true);
    config.enableDruidBuffs = sConfigMgr->GetOption<bool>("RndbotBuffs.Classes.Druid", true);
    config.enablePaladinBuffs = sConfigMgr->GetOption<bool>("RndbotBuffs.Classes.Paladin", true);
    config.debug = sConfigMgr->GetOption<bool>("RndbotBuffs.Debug", false);

    LOG_INFO(
        "server.loading",
        "mod-rndbot-buffs: {} (interval={} ms, radius={:.1f}, minimum mana={:.1f}%)",
        config.enabled ? "enabled" : "disabled",
        config.checkIntervalMs,
        config.radius,
        config.minimumManaPct);
}

bool HasAnyAura(
    PlayerbotAI* botAI,
    Player* target,
    std::initializer_list<char const*> auraNames)
{
    for (char const* auraName : auraNames)
    {
        if (botAI->HasAura(auraName, target))
            return true;
    }

    return false;
}

bool TryCastBuff(
    PlayerbotAI* botAI,
    Player* target,
    char const* spellName,
    std::initializer_list<char const*> equivalentAuras)
{
    if (!botAI->HasSpell(spellName) || HasAnyAura(botAI, target, equivalentAuras))
        return false;

    if (!botAI->CanCastSpell(spellName, target))
        return false;

    if (!botAI->CastSpell(spellName, target))
        return false;

    if (config.debug)
    {
        LOG_DEBUG(
            "playerbots",
            "[RndbotBuffs] {} cast '{}' on {}",
            botAI->GetBot()->GetName(),
            spellName,
            target->GetName());
    }

    return true;
}

bool HasPaladinBlessing(PlayerbotAI* botAI, Player* target)
{
    return HasAnyAura(
        botAI,
        target,
        {
            "blessing of kings",
            "greater blessing of kings",
            "blessing of might",
            "greater blessing of might",
            "blessing of wisdom",
            "greater blessing of wisdom",
            "blessing of sanctuary",
            "greater blessing of sanctuary"
        });
}

bool TryCastPaladinBuff(PlayerbotAI* botAI, Player* target)
{
    if (HasPaladinBlessing(botAI, target))
        return false;

    if (TryCastBuff(
            botAI,
            target,
            "blessing of kings",
            { "blessing of kings", "greater blessing of kings" }))
    {
        return true;
    }

    bool const targetIsCaster = PlayerbotAI::IsCaster(target, true);
    char const* preferredSpell = targetIsCaster ? "blessing of wisdom" : "blessing of might";
    char const* fallbackSpell = targetIsCaster ? "blessing of might" : "blessing of wisdom";

    if (TryCastBuff(botAI, target, preferredSpell, { preferredSpell }))
        return true;

    return TryCastBuff(botAI, target, fallbackSpell, { fallbackSpell });
}

bool TryCastClassBuff(PlayerbotAI* botAI, Player* target)
{
    Player* bot = botAI->GetBot();

    switch (bot->getClass())
    {
        case CLASS_PRIEST:
            if (!config.enablePriestBuffs)
                return false;

            return TryCastBuff(
                       botAI,
                       target,
                       "power word: fortitude",
                       { "power word: fortitude", "prayer of fortitude" }) ||
                   TryCastBuff(
                       botAI,
                       target,
                       "divine spirit",
                       { "divine spirit", "prayer of spirit" }) ||
                   TryCastBuff(
                       botAI,
                       target,
                       "shadow protection",
                       { "shadow protection", "prayer of shadow protection" });

        case CLASS_MAGE:
            return config.enableMageBuffs &&
                   TryCastBuff(
                       botAI,
                       target,
                       "arcane intellect",
                       { "arcane intellect", "arcane brilliance" });

        case CLASS_DRUID:
            if (!config.enableDruidBuffs)
                return false;

            return TryCastBuff(
                       botAI,
                       target,
                       "mark of the wild",
                       { "mark of the wild", "gift of the wild" }) ||
                   TryCastBuff(botAI, target, "thorns", { "thorns" });

        case CLASS_PALADIN:
            return config.enablePaladinBuffs && TryCastPaladinBuff(botAI, target);

        default:
            return false;
    }
}

bool IsIdleRndbot(Player* bot, PlayerbotAI* botAI)
{
    if (!bot || !botAI || !bot->IsInWorld() || !bot->IsAlive() ||
        bot->IsDuringRemoveFromWorld() || bot->IsBeingTeleported())
    {
        return false;
    }

    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
        return false;

    if (bot->IsInCombat() || bot->GetVictim() || botAI->GetState() == BOT_STATE_COMBAT)
        return false;

    if (bot->IsNonMeleeSpellCast(true) || bot->IsMounted() ||
        bot->getStandState() != UNIT_STAND_STATE_STAND || botAI->IsInVehicle())
    {
        return false;
    }

    if (config.skipBattlegrounds && bot->InBattleground())
        return false;

    if (bot->GetMaxPower(POWER_MANA) == 0 ||
        bot->GetPowerPct(POWER_MANA) < config.minimumManaPct)
    {
        return false;
    }

    return true;
}

bool IsEligibleTarget(Player* bot, Player* target)
{
    if (!target || target == bot || !target->GetSession() ||
        target->GetSession()->IsHeadless() || !target->IsInWorld() ||
        !target->IsAlive() || target->IsGameMaster() ||
        target->IsDuringRemoveFromWorld() || target->IsBeingTeleported())
    {
        return false;
    }

    if (config.sameFactionOnly && target->GetTeamId() != bot->GetTeamId())
        return false;

    if (config.skipPlayersInCombat && target->IsInCombat())
        return false;

    if (bot->GetDistance(target) > config.radius)
        return false;

    return bot->IsWithinLOS(
        target->GetPositionX(),
        target->GetPositionY(),
        target->GetPositionZ());
}

std::vector<Player*> FindNearbyPlayers(Player* bot)
{
    std::vector<std::pair<float, Player*>> playersByDistance;
    Map::PlayerList const& mapPlayers = bot->GetMap()->GetPlayers();

    for (Map::PlayerList::const_iterator itr = mapPlayers.begin(); itr != mapPlayers.end(); ++itr)
    {
        Player* target = itr->GetSource();
        if (!IsEligibleTarget(bot, target))
            continue;

        playersByDistance.emplace_back(bot->GetDistance(target), target);
    }

    std::sort(
        playersByDistance.begin(),
        playersByDistance.end(),
        [](auto const& left, auto const& right)
        {
            return left.first < right.first;
        });

    std::vector<Player*> players;
    players.reserve(playersByDistance.size());
    for (auto const& [distance, player] : playersByDistance)
    {
        (void)distance;
        players.push_back(player);
    }

    return players;
}

void TryBuffNearbyPlayer(Player* bot)
{
    PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
    if (!IsIdleRndbot(bot, botAI))
        return;

    for (Player* target : FindNearbyPlayers(bot))
    {
        if (TryCastClassBuff(botAI, target))
            return;
    }
}

class RndbotBuffPlayerScript final : public PlayerScript
{
public:
    RndbotBuffPlayerScript()
        : PlayerScript(
              "RndbotBuffPlayerScript",
              { PLAYERHOOK_ON_AFTER_UPDATE, PLAYERHOOK_ON_LOGOUT })
    {
    }

    void OnPlayerAfterUpdate(Player* player, uint32 diff) override
    {
        if (!config.enabled || !player || !player->GetSession() ||
            !player->GetSession()->IsHeadless())
        {
            return;
        }

        ObjectGuid::LowType const guid = player->GetGUID().GetCounter();

        {
            std::lock_guard<std::mutex> guard(botCheckTimers_mutex);

            auto [timerItr, inserted] = botCheckTimers.try_emplace(
                guid,
                urand(0, config.checkIntervalMs));

            if (!inserted)
                timerItr->second += diff;

            if (timerItr->second < config.checkIntervalMs)
                return;

            timerItr->second = urand(0, std::min<uint32>(1000, config.checkIntervalMs / 4));
        }

        TryBuffNearbyPlayer(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
        {
            std::lock_guard<std::mutex> guard(botCheckTimers_mutex);
            botCheckTimers.erase(player->GetGUID().GetCounter());
        }
    }
};

class RndbotBuffWorldScript final : public WorldScript
{
public:
    RndbotBuffWorldScript()
        : WorldScript("RndbotBuffWorldScript", { WORLDHOOK_ON_BEFORE_CONFIG_LOAD })
    {
    }

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        LoadConfig();
    }
};
}

void AddRndbotBuffScripts()
{
    new RndbotBuffPlayerScript();
    new RndbotBuffWorldScript();
}
