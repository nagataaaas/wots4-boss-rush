# Boss Rush

[日本語](README.md) | [Changelog](CHANGELOG.en.md)

> [!NOTE]
> This Mod is for the [Steam version of Way of the Samurai 4](https://store.steampowered.com/app/312780/4/?l=english) only.

Once you have seen the true route's ending, "The Light of Hope", the Specter in the graveyard offers "Fight the strong". Fight the strong of the three factions and of the town one after another, in an empty graveyard.

<!-- Gameplay video -->

## Features

### When "Fight the strong" appears

"Fight the strong" is added to the Specter's choices in the graveyard when all of the following are true:

- You have seen the ending "The Light of Hope" (in this playthrough or any earlier one)
- No boss rush is under way

Choosing "Fight the strong" opens a choice of opponents:

- If you have never cleared either of the occupations "Four Samurai Lords vs. Yakuza Quartet" and "The Amihama Eight", the three factions (the Prajnas, the Shogunate and the foreigners) are offered right away.
- If you have cleared either of them, you first choose between "Fight the strong of the three factions" and "Fight the strong of the town". What the town offers depends on the occupations you have cleared.

| Choice | Offered when |
| --- | --- |
| Fight the strong of the Prajnas | Always |
| Fight the strong of the Shogunate | Always |
| Fight the strong of the foreigners | Always |
| Fight the Four Samurai Lords (except Kumagami) | You have cleared the occupation "Four Samurai Lords vs. Yakuza Quartet" |
| Fight the Yakuza Quartet | You have cleared the occupation "Four Samurai Lords vs. Yakuza Quartet" |
| Fight the Amihama Eight | You have cleared the occupation "The Amihama Eight" |

Occupations cleared in earlier playthroughs count too.

### Who you fight

You fight the stages in order from 1. The strong of one stage come in together.

| Choice | 1 | 2 | 3 | 4 | 5 |
| --- | --- | --- | --- | --- | --- |
| Fight the strong of the Prajnas | Jinrai Kogure | Madara | Reddo Akagi | | |
| Fight the strong of the Shogunate | Shigeru Moro (with a sword) | Hikaru Kotobuki | The Kinugawa Sisters (Mayu, Chika and Yuri together) | Onsen Kinugawa | Onsen Kinugawa (after the boiling) |
| Fight the strong of the foreigners | Melinda | New Commander (in armor) | J.J. | | |
| Fight the Four Samurai Lords (except Kumagami) | Isshinsai Kotetsu | Izo Orochi | Zenki Kirisame | | |
| Fight the Yakuza Quartet | Daigoro Suzume | Gozo Hiyoko | Kai Kobato | Yorozumaru Mozu | |
| Fight the Amihama Eight | The Deathbringer Boss | The Four Samurai Lords (Kotetsu, Orochi, Kumagami and Kirisame together) | The Yakuza Quartet (Suzume, Hiyoko, Kobato and Mozu together) | The Commander of the Invading Troops | |

### How a boss rush goes

- After your choice, "The strong await you in the graveyard..." appears and you are taken into an empty graveyard. No time passes on the way.
- Each stage starts with the names of your opponents. They come in from behind the camera and head for you.
- Once you defeat every opponent of a stage, the next stage starts 3 seconds later. After the last stage, "You have defeated all the strong." appears.
- During a boss rush, you cannot walk out of the graveyard. Walking out after defeating them all takes you back to the usual graveyard. Fast travel takes you where you chose.
- If you fall, the boss rush ends there and the game goes on as usual.
- If no opponent comes near you for 60 seconds (for example, one appeared where it cannot reach you), the opponents vanish and the boss rush ends.
- The strong you defeat in a boss rush do not die in the story. Who lives and how the story goes do not change.

## Installation

### Automatic installation (recommended)

1. Close the game.
2. Download and run [WayOfTheSamurai4ModInstaller.exe](https://github.com/nagataaaas/wots4-mods/releases/latest/download/WayOfTheSamurai4ModInstaller.exe).
   - If "Windows protected your PC" appears, click "More info" → "Run anyway".
   - When asked whether to allow it to make changes as administrator, click "Yes" (it writes to the game folder).
3. Follow the screen and choose `boss_rush` in the list of Mods. Move with Up and Down, choose with Space and confirm with Enter.

The ModLoader is installed along with it. The same steps update the Mod.

<details>
<summary>Manual installation</summary>

> [!WARNING]
> Install [wots4-mods (ModLoader)](https://github.com/nagataaaas/wots4-mods) first. The ModLoader must be v2026.10.19 or later.

1. Close the game.
2. Download `wots4-mod-boss-rush-<version>.zip` from [Releases](../../releases) and extract it.
3. Copy the extracted `mods` folder into the game's installation folder (the folder containing `WayOfTheSamurai4.exe`). If a `mods` folder already exists, merge the contents into it.
4. Confirm that `{game installation folder}/mods/boss_rush/mod.dll` exists, then start the game.

</details>

## Uninstallation

1. Close the game.
2. Delete the `{game installation folder}/mods/boss_rush/` folder.

The Mod adds nothing to your saves, so they keep working after you remove it.
