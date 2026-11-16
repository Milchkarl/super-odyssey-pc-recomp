# FSR 4 on the Steam Deck: what works, what doesn't, and why (2026-11-16)

A reference for this port and other Switch recomps that render with **Vulkan** and want AMD FSR 4 / 4.1 on the Steam
Deck or other SteamOS devices. Everything below was tested on a Steam Deck OLED (GPU `1002:1435`, Mesa RADV) and a
Windows PC; the numbers and log lines are from those runs.

## Short answer

FSR 4.1.2 (including the lightweight "Lite" model) can't run in a Vulkan game on the Steam Deck today - not natively
and not through Proton - because AMD's runtime only offers it with AMD's own Windows driver. The Deck keeps FSR 2
(native, in the Linux build) for now.

## What AMD ships

- **AMD FSR SDK 2.4.0** (2026-10-28, `GPUOpen-LibrariesAndSDKs/FidelityFX-SDK`) brings FSR Upscaling 4.1.2 to RDNA 2:
  Radeon RX 6000, and with a new lightweight INT8 model to handheld APUs including the **Ryzen Z2 A** (RDNA 2, the Steam
  Deck class of chip). Vulkan is listed at parity with DX12.
- The model is chosen by the runtime from the hardware; the game doesn't pick "Lite". AMD's figures for the Ryzen Z2 A:
  **5.6 ms per frame at 1920x1080** output (about 2.8 ms at the Deck's 1280x800 by pixel count).
- It comes only as signed **Windows** DLLs (`amd_fidelityfx_loader_vk.dll`, `amd_fidelityfx_upscaler_vk.dll`). There is
  no Linux build, so a native Linux program (this port's AppImage) can't load it at all.

This port already ships those SDK 2.4.0 DLLs in its Windows build (`tools/fetch_upscalers.sh`) and asks the runtime
which upscaler versions it offers (`runtime/nvn/vulkan/fsr3_sdk.c`, `vk_fsr3_init`).

## What was tested

1. **Windows build under Proton Experimental on the Deck** (the route the Decky plugin `db1522/fsr4-lite-steam-machine`
   uses, but without OptiScaler: our game calls AMD's API itself). The build ran; AMD's runtime offered:

   ```
   [vk] using AMD Custom GPU 0932 (RADV VANGOGH)
   [fsr-sdk] provider ...: 3.1.6
   [fsr-sdk] provider ...: 2.3.4
   [fsr4] no ML provider supports this GPU; FSR 3.1 fallback is available
   ```

   No FSR 4 provider. The same happens on a desktop Ryzen's small RDNA 2 iGPU on Windows with AMD's own driver (that chip
   isn't on AMD's list), while the Deck's chip is - so on the Deck the refusal comes from the driver (RADV), not the chip.
2. **Forcing the version**: the API's `ffxOverrideVersion` only accepts ids the query returned
   (`ffx_api.h`: "Must be a value returned from a query"), so an FSR 4 id can't be requested when it isn't offered.
3. **Speed under Proton**: the Windows build ran at ~8 FPS on the Deck in its first minutes (shaders compiling with no
   prepared cache, plus Wine), far below the native Linux build's 60. Even with a prepared cache, Proton on the Deck's
   4 cores is unlikely to beat the native build.

## Why the Decky plugin works on the Deck and we don't

The plugin targets **DirectX 12** games. Under Proton, DX12 runs on vkd3d-proton, which (as far as we know) provides the
part of AMD's Windows driver that FSR 4's DX12 path calls, implemented on RADV. Nothing provides that for a game that
talks **Vulkan** to AMD's Vulkan runtime directly - which is what this port, and most Switch recomps, do. OptiScaler sits
on top of the same runtimes, so it can't add what the runtime refuses.

## What would make it possible

- AMD offering FSR 4 through its Vulkan runtime on Mesa RADV (or a Linux runtime): then the code already in
  `fsr3_sdk.c` picks it up with no change (it takes the first 4.x provider the query returns). Re-test with each new SDK.
- Or a Proton/Wine component that answers AMD's Vulkan runtime the way vkd3d-proton answers its DX12 path.
- Not recommended: making AMD's runtime believe it runs on AMD's driver (faking the GPU/driver it queries through
  `vkGetPhysicalDeviceProperties2`). Unsupported, likely against AMD's terms, and the ML shaders may need features RADV
  doesn't expose on RDNA 2.

## Recommendation for the Deck meanwhile

- Keep **FSR 2** (native, `runtime/nvn/vulkan/fsr2.c`) as the Deck's upscaler; extending it to Handheld mode is the next
  step.
- Mind the budget: the Deck is GPU-bound at its 15 W limit, so any ML upscaler costing ~3 ms a frame only pays off if a
  lower render resolution saves more than that.

## How to re-test quickly

On the Deck, with the Windows build in a test folder and Proton Experimental installed, run it with
`STEAM_COMPAT_DATA_PATH`/`STEAM_COMPAT_CLIENT_INSTALL_PATH` set and `.../Proton - Experimental/proton run SuperOdysseyRecomp.exe`,
then look for the `[fsr-sdk] provider` lines in its log. A `4.x` line there means FSR 4 is available.
