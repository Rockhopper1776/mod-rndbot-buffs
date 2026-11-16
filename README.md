# RandomBot Buff Module
# mod-rndbot-buffs
# Created by Rockhopper1776

An AzerothCore module for
[mod-playerbots](https://github.com/mod-playerbots/mod-playerbots) that lets
idle random bots cast useful class buffs on nearby real player characters.

The module is intentionally conservative:

- only rndbots managed by `RandomPlayerbotMgr` participate;
- the bot must be alive, unmounted, out of combat, and not casting;
- the bot must remain above a configurable mana threshold;
- only real, living, visible players in range are considered;
- by default, targets must be same-faction and out of combat;
- each bot casts at most one buff per check;
- existing single-target and group versions of a buff are respected;
- paladins do not replace any blessing already on the player.

## Supported buffs

| Rndbot class | Buffs |
| --- | --- |
| Priest | Power Word: Fortitude, Divine Spirit, Shadow Protection |
| Mage | Arcane Intellect |
| Druid | Mark of the Wild, Thorns |
| Paladin | Blessing of Kings, or Wisdom/Might based on the target's role |

The module asks `PlayerbotAI` to resolve and cast the spell, so the bot uses the
highest rank it knows and keeps the normal range, mana, cooldown, reagent, and
target validation performed by mod-playerbots.

## Requirements

- [mod-playerbots/azerothcore-wotlk](https://github.com/mod-playerbots/azerothcore-wotlk)
  on its `Playerbot` branch
- [mod-playerbots/mod-playerbots](https://github.com/mod-playerbots/mod-playerbots)
  on its `master` branch

## Installation

Place this directory beside `mod-playerbots`:

```text
azerothcore-wotlk/
└── modules/
    ├── mod-playerbots/
    └── mod-rndbot-buffs/
```

Re-run CMake, rebuild the server, and install it. AzerothCore will install
`mod_rndbot_buffs.conf` with the other module configuration files.

Edit `mod_rndbot_buffs.conf` if you want to change the
defaults, then restart the worldserver. Configuration reloads are also
supported through AzerothCore's normal config reload path.

## Configuration

The defaults check every five seconds, search 30 yards, and preserve 35% mana.
See [`conf/mod_rndbot_buffs.conf.dist`](conf/mod_rndbot_buffs.conf.dist) for all
settings and comments.

## Compatibility target

The module targets the `Playerbot` branch of mod-playerbots/azerothcore-wotlk
and the corresponding `master` version of mod-playerbots.

The core must provide `WorldSession::IsHeadless()`, which replaced
`WorldSession::IsBot()` in the
[September 23, 2026 core update](https://github.com/mod-playerbots/azerothcore-wotlk/commit/d03021d845b21fb1fbba093c8e15c1ce5746911b).
Older core revisions that expose only `IsBot()` are not compatible with this
version of the module. Use matching core and mod-playerbots revisions.

## License

GNU Affero General Public License v3.0 or later.
