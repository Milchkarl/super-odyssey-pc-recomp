## New

- **Super Odyssey Configuration** (#173): a separate program next to the game (`Super Odyssey Configuration.exe` on Windows; on Linux and the Steam Deck the app's "Super Odyssey Configuration" entry or the `--configure` launch option) that opens all the settings without starting the game.
- **Restore defaults** (#173): on the start page and in the configuration program. Every setting goes back to the default - the game as on the Switch, fullscreen at your display's resolution. Your controls, languages, graphics card and volume are kept, and your old settings are saved as a backup file next to `settings.ini`.

## Fixed

- **Crash in the New Donk City sewer on Linux** (#181): "Multithreaded rendering" set to Auto by older versions turned itself on with NVIDIA cards and could crash the graphics driver. Auto now means Off, as for new installs.
- **Spin throw key above 60 FPS** (#185): it now works the same at any frame rate.
- **DLSS greyed out on some NVIDIA PCs** (#189): the game now also finds DLSS when Windows still points to an older driver's folder.
- **Internal resolution on the start page** (#161): the sizes shown now match your screen's shape (for example 3840 x 1600 on an ultrawide).
- **Compiling no longer takes every CPU thread** (#186): one is left free, so the PC stays usable.
- If the game ever stops responding, its log now says where, to help us fix freezes like the old-save one (#188).

No game code rebuild or shader preparation after updating from 1.0.4.

Updates will be released weekly to maintain the project. Thank you for the reports and logs!
