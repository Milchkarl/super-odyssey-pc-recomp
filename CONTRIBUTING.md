# Contributing to Super Odyssey PC Recomp

Bug reports, testing on different hardware and code contributions are welcome.

## The one hard rule: no Nintendo files

Nothing owned by Nintendo may ever be committed or attached to an issue or pull request:

- no copies of the game in any form (game packages or extracted folders)
- no extracted game files (`extracted/`), no generated code (`generated/`), no game code library
  (`odyssey_game.dll`), no fonts (`.bfttf` / Switch `.ttf`)
- no game assets of any kind: textures, models, music, text, level files - also not "just one file for testing"

`.githooks/pre-commit` refuses such files (`git config core.hooksPath .githooks`). Screenshots and short clips of the
game in bug reports are fine. Logs are fine too. Requests for game files are closed.

## Reporting bugs

Please include:

- what you did, what happened and what you expected, and where in the game (a screenshot or a short clip helps)
- your game version (1.0.0 or 1.4.1), the port's version, your OS, CPU, GPU and driver version
- the log: `SuperOdysseyRecomp.log` in the install folder (it names the crash and the game's call chain)
- after a crash, the **crash report**: the error box has a **Copy crash report** button
- for a graphics glitch, press **F9** where it happens: the frame's details go into the log
- a save from just before the problem helps a lot

## Code

- Match the style of the surrounding code; comments say why, not what.
- The game is the reference: a fix should make the port behave like the Switch, not hide a symptom. The debugging
  tools are environment variables (`ODYSSEY_*`), documented where they are read.
- Menu text: every new option needs a plain-English note in `runtime/ui/option_notes.h` (what it does, what is
  recommended), and its strings in `runtime/ui/lang/*.txt` (Korean, Brazilian Portuguese).
- Test a change in the game before sending it (`tools/odyssey_run.sh`, a stage warp: `ODYSSEY_WARP`), and say in the
  pull request what you tested and on which hardware.

## How pull requests are handled

Every pull request is read line by line for anything harmful, then built and played on **Windows and a Steam Deck**
before it is merged. Nothing from a contributor ships untested, and releases go out only after the release build
itself has passed those tests (`tools/release.sh`).
