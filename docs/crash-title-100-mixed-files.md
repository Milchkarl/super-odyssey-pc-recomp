# Crash at the title screen on 1.0.0: game files from two versions (report 2026-11-21 01:46 UTC)

Report: `super-odyssey-pc-recomp-report-147097-report-20261121-014616-29204-0.txt` (0.5.2, Windows build 26300,
Ryzen 9 5900X, RX 9060 XT, game 1.0.0). `exception c0000005 accessing 0x8` in `f_7000800ef8+0x9d`, frame 111,
guest lr `TitleMenuScene::exeAppear+0x154`.

## Cause (reproduced)

The player's game files are mixed: the game code (exefs) is 1.0.0, the romfs is not 1.0.0's. Their log says
`[fs] romfs index: 6911 files`; 1.0.0's romfs has 4,636 and 1.4.1's 4,972. The 1.0.0 code and that data don't match.

What happens, in the 1.0.0 code:
- `f_7000800ef8` is `al::AnimPlayerSkl::isSklAnimEnd(int)`. It reads slot 0's animation, which is null: `[null + 8]`.
- `TitleMenuScene::exeAppear` (0x70005061c0): step 2 starts the menu layout's action "Boot"; step 150 shows the title
  Mario and starts his action "PauseMenuStart"; from step 3 on, once `al::isActionEnd(layout, "Boot")` is true, it asks
  `al::isActionEnd(MarioActor)` (+0x150, returns to +0x154).
- `al::isActionEnd(IUseLayoutAction*, name)` returns **true when the layout has no such animation**.
- 1.0.0's `LayoutData/Menu.szs` has `anim/Menu_Boot.bflan`. Later versions' Menu.szs has none (`Menu_BootAppear`,
  `Menu_BootWait`, `Menu_BootEnd` instead), and 1.4.1's exeAppear no longer asks about Mario this way.
- So with an update's Menu.szs, "Boot" counts as ended on step 3, and the game asks about Mario's animation before
  he has one (ActionAnimCtrl index -1, AnimPlayerSkl slot 0 empty): the crash. It fits the report: the title first drew
  at frame 106, the crash was at frame 111.

Reproduced on the PC (0.5.3 runtime, test copies only):

| Game code | Game data (romfs) | Frame rate | Result |
|---|---|---|---|
| 1.0.0 | 1.0.0 (4,636 files) | 165 setting (120 drawn) | title for 3+ min, no crash |
| 1.0.0 | 1.4.1 (4,972 files) | **60** | crash: `f_7000800ef8+9d`, lr `TitleMenuScene::exeAppear+154`, same as the report |

## Ruled out

- 165 FPS / the native-120 path: the mixed files crash at 60 FPS too; clean 1.0.0 runs at 165.
- skip_intro, PlayStation prompts, the HUD layer: on in both runs above (the reporter's settings.ini), crash only with
  the mixed files. The `[hud]` and `[linear]` lines before the crash are the title's normal draws.
- Not fixed by 0.5.3: nothing there touches this (it isn't in docs/crash-triage-2026-11-20.md either). The game code is
  doing what 1.0.0 does with the files it is given.

## The fix (runtime/services/fs.c, `rom_check_version`)

With 1.0.0's game code (build id 3ca12dfaaf9c82da), a romfs holding `EffectData/EffectPtclPatch.szs` or
`EventData/BalloonGame.szs` (files later updates added; 1.0.0's romfs has neither, 1.4.1's has both and 334 more)
stops at start with a message: the files don't match, extract one version, delete `extracted`. The same way as the
existing "romfs of an update only" check. `ODYSSEY_ALLOW_MIXED_FILES=1` skips it (tests).

Checked: 1.0.0 + 1.4.1 romfs shows the message (no crash); clean 1.0.0 at the 165 setting reaches the title and runs.
1.4.1 is not affected (the check is for 1.0.0's code only). Not covered: 1.4.1's code with 1.0.0's romfs (not seen).

Not done: the reporter's own files are unknown (6,911 files is more than either version; perhaps a dump with the
update merged in, or extra files). The message asks them to extract again.

## Draft reply (for the owner to approve; not posted)

> Thanks for the report. The crash comes from the game files: the game code is version 1.0.0, but the romfs is from a
> later update (yours has 6,911 files; 1.0.0's has 4,636). The 1.0.0 code looks for a title-screen animation that the
> newer files don't have, and crashes. To fix it, extract the game again from one version: the base game 1.0.0 alone,
> or 1.4.1 (base game with the 1.4.1 update applied). Then delete the `extracted` folder in your game folder and start
> again so the setup asks for the files. The next version will say this at start instead of crashing.
