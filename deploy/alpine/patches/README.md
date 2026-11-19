# deploy/alpine/patches

Build-time patches applied to **submodule** working trees before the
Alpine build. Submodules cannot carry fork commits (their upstream
repositories are not ours), so the fork carries the patches here and the
build scripts (`scripts/build-alpine.sh`, and the image build's
`build/25-launcher.sh` in cardputer-zero-alpine) apply them idempotently
(`patch --dry-run` first, skip when already applied). The pinned submodule
commit itself is never modified — a fresh checkout reproduces exactly
this state.

## calculator-native-fbdev.patch

Upstream `projects/Calculator/SConstruct` hardwires **every non-cross
build to the SDL simulator**: the `else` branch of the cross check writes
`CONFIG_V9_5_LV_USE_SDL=y` into `build/config/config_tmp.mk`, and only
the glibc cross profile gets the device settings (FBDEV + EVDEV + NEON).
There was no working native-device path at all — upstream only ever built
the Calculator via cross-compile (glibc sysroot) or as a host simulator.

The patch makes the non-cross branch host-aware: on `aarch64` it writes
the same device config the cross profile uses (FBDEV + EVDEV + NEON and
`LV_USE_DRAW_SW_ASM`), and drops the stale generated Kconfig files when
`config_tmp` changes so the settings actually take effect
(`selected-defaults.txt` only guards the profile file, not `config_tmp` —
without this a host-SDL config sticks around and `main.cpp` fails on
`SDL2/SDL_render.h`). x86_64 hosts keep the SDL simulator unchanged.

If upstream wires its own native-device path one day, drop this patch and
re-pin the submodule.
