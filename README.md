# launcher — Alpine edition

APPLaunch, the graphical launcher UI for the **M5Stack CardputerZero**, ported
to **Alpine Linux (musl/OpenRC)** — the userspace of the
[cardputer-zero-alpine](https://github.com/kkazakov/cardputer-zero-alpine-linux)
image. This is a fork of [CardputerZero/launcher](https://github.com/CardputerZero/launcher)
kept deliberately close to upstream: `master` tracks upstream untouched, all
Alpine work lives on the `alpine` branch, and the diff is small enough to
rebase over every upstream release.

The UI is the upstream product, unchanged: the 320×170 carousel home, status
bar (clock, battery, Wi-Fi), global shortcuts, lock screen, and every
navigation behavior you know from the stock image.

## What is different from upstream

Product scope — this launcher ships on a lean Alpine image, so the product
surface is deliberately smaller:

- **No application store.** The `Store` built-in entry and the AppStore
  project are gone; the image has no store backend, and nothing on the home
  carousel links to one.
- **No demo apps.** HelloWorld, UserDemo and the other upstream example/product
  projects (ZClaw, LaunchWizard, CameraApp, Keyboard-Guide) are removed from
  this fork. The device ships exactly two binaries: the launcher and the
  Calculator.
- **No Python terminal.** The built-in `Python` entry required a
  preinstalled `python3`; the image does not carry one. The **CLI** terminal
  (an interactive `bash` over a PTY) remains.
- **Settings, where applicable.** The full settings tree ships; entries whose
  backends only exist on the upstream Raspberry Pi OS image are hidden
  instead of shown failing:
  - *System → Software* (systemd/dpkg update channel) — hidden on Alpine.
  - *Developer → ADB* (USB gadget mode) — hidden on Alpine.
  - *Date & Time → Set Manually* — works: the port adds a fallback to
    `settimeofday` when systemd's `timedate1` dbus service is absent, and
    the existing *Write RTC?* step persists it to the RX8130.
  - *Network Time* — reports unavailable on Alpine (no `timedate1`);
    keep the clock via the manual set or NTP configured in the base image.
  - *Speaker* (volume keys and the page) — works over ALSA: the port adds
    an `amixer` DACL/DACR fallback to the PulseAudio mixer path, which is
    the only backend the Alpine image has.
- **Home screen** carries Settings, CLI, SSH, IP Panel, Calculator and the
  **HDMI** desktop toggle; the upstream demo games (Snake, Tank), the Store
  and the Python terminal are not part of this fork's product. The home
  background stays the stock one; the fork's Alpine artwork lives on the
  **lock screen** (`lockscreen.png`, 320x150, drawn under the lock clock).
- **HDMI desktop toggle** — an in-process page (`ui_app_hdmi`) that starts
  and stops the on-demand HDMI session (`cpz-desktop start|stop`) with a
  single switch: status is polled from the real session (sway), the choice
  is persisted in `/var/lib/applaunch/hdmi-desktop` and re-applied at boot
  by `/etc/local.d/cpz-hdmi-boot.start`. HDMI stays dark until it starts.
- **Runs as the unprivileged `cpz` user** on the Alpine image (OpenRC
  `command_user="cpz:cpz"`): cpz is in `video`/`input`/`audio`/`i2c`/`seat`
  and has passwordless sudo, so the panel, keypad, ALSA mixer, BME/RTC i2c
  chips, RTC writes, power actions and the desktop session all work without
  running the UI as root. `cp0_lvgl`'s power commands now pick `systemctl`
  when present and the OpenRC `reboot`/`poweroff` verbs (via `sudo -n`)
  otherwise — upstream's hardwired `systemctl` was a no-op on Alpine.

Platform ports (the interesting bits):

- `cp0_lvgl` — the platform layer — now builds on **musl**. The LVGL runner
  loop's glibc-only `sem_clockwait()` was replaced by the portable
  condition-variable wake path (the same code macOS builds had used all
  along; wake counting and monotonic deadlines preserved).
- Wi-Fi/Ethernet settings drive **NetworkManager** (`nmcli`) — the base
  Alpine image therefore runs NetworkManager (as the stock M5 image does),
  not ifupdown-ng.
- Audio is **ALSA** (ES8389): Miniaudio falls back from PulseAudio (dlopen,
  absent) to `libasound`; the Speaker page and volume keys use `amixer`.
- Battery reads the **BQ27220** fuel gauge directly over I²C; the keyboard
  is raw evdev (tca8418) via libinput + xkbcommon, with the SYM layer loaded
  from `/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map` when present.
- The framebuffer auto-detects the ST7789 fbtft device (`/proc/fb` scan),
  so it works whether the panel is fb0 or fb1.

Repository shape:

- Removed: `projects/{AppStore,HelloWorld,UserDemo,ZClaw,LaunchWizard,CameraApp,Keyboard-Guide}`,
  the `.deb` packaging pipeline (`scripts/debian_packager.py`, release
  workflow), and the packaging contract tests that guarded them.
- Kept: `projects/APPLaunch` (core), `projects/Calculator` (submodule),
  `projects/CardputerZero-Emulator` (desktop/web UI preview — it needs no
  device and was kept for development), `SDK` and `Calculator` submodules at
  their upstream pins, and the full `ext_components/` platform tree.
- Upstream's SDK (M5Stack_Linux_Libs) is used **unmodified**; deeper SDK
  work (musl cross sysroot etc.) is planned for phase 2.

## Building (Alpine target)

From a host with Docker and arm64 binfmt (the same prerequisite as the
cardputer-zero-alpine image build):

```sh
./scripts/build-alpine.sh              # → dist-alpine/applaunch-1.0.0-alpine.tar.gz
./scripts/build-alpine.sh 1.0.1-alpine  # custom APPLAUNCH_VERSION
```

The script builds **natively** inside a `linux/arm64 alpine:3.24` container
(emulated with qemu on x86 hosts): the SCons toolchain picks the device
profile (`config_defaults.mk`, FBDEV + EVDEV) automatically on aarch64, and
every library (freetype, libcurl, libinput, xkbcommon, udev, dbus, gio,
libstdc++) is the exact musl build the device image runs. This is why the
upstream glibc cross-sysroot path is not used here — it cannot produce musl
binaries.

One submodule carries a build-time patch (see
[deploy/alpine/patches/README.md](deploy/alpine/patches/README.md)):
upstream's Calculator had no native-device build path (every non-cross build
was hardwired to the SDL simulator). The build scripts apply the patch
idempotently; the submodule pin itself is untouched.

CI (`.github/workflows/alpine-build.yml`) runs the same build on GitHub's
arm64 runners and publishes the tree as an artifact.

### Other build profiles (upstream compatibility)

All upstream profiles still work — SDL2 simulator, cross-compile, macOS,
Windows. See the [upstream README](https://github.com/CardputerZero/launcher)
for their tooling requirements; the SDK and profiles are untouched.

## Deploying on Alpine

The tarball contains one `APPLaunch/` tree:

```sh
tar -xzf applaunch-1.0.0-alpine.tar.gz -C /usr/share/
# → /usr/share/APPLaunch/{bin,share,applications,lib,adb}
```

OpenRC integration (replaces upstream's systemd user service):

```sh
cp deploy/alpine/applaunch.initd  /etc/init.d/applaunch
cp deploy/alpine/applaunch.confd /etc/conf.d/applaunch
rc-update add applaunch default
rc-service applaunch start
```

`deploy/alpine/runtime-packages.txt` lists the Alpine packages the binary
and its services need. The launcher runs as the unprivileged **`cpz`**
user: the image puts cpz in `video`/`input`/`audio`/`i2c`/`seat` and
grants passwordless sudo, so the panel, keypad, ALSA mixer, i²c chips,
RTC writes, power actions and the on-demand session work like upstream's
polkit flow — without running the UI as root.

Console layout on the device: the internal LCD is owned by the launcher;
there is no VT getty and no kernel console on the VTs, so **HDMI stays
dark until the desktop session starts** (`cpz-desktop`/the HDMI app).
Serial is the local rescue console. Do not run a getty on the panel and
do not blank fbcon while the launcher is up — fbcon blanking turns the
fbtft backlight off.

The cardputer-zero-alpine image build consumes all of this automatically via
`build/25-launcher.sh`.

## Project structure

```
launcher/
├── SDK/                        # M5Stack_Linux_Libs SDK (git submodule, upstream pin)
├── projects/
│   ├── APPLaunch/              # the launcher (core)
│   │   ├── main/               #   src + ui (carousel, pages, settings)
│   │   ├── APPLaunch/           #   runtime resource tree (copied to dist/)
│   │   └── tests/              #   C++ + python source-contract tests
│   ├── Calculator/             # calculator (git submodule)
│   └── CardputerZero-Emulator/ # desktop/web UI preview (CMake)
├── ext_components/            # cp0_lvgl platform layer + vendored libs
├── deploy/alpine/              # OpenRC units + runtime package list
├── scripts/build-alpine.sh     # Alpine build orchestrator
└── docs/                       # upstream architecture guides (see AGENTS.md
                                # for what is stale)
```

## Syncing with upstream

```sh
git remote add upstream https://github.com/CardputerZero/launcher.git
git fetch upstream
git checkout master && git merge --ff-only upstream/master   # keep master = upstream
git checkout alpine && git rebase master                     # replay the port
# resolve, run ./scripts/build-alpine.sh + the test suites, push:
git push origin master alpine
```

The fork is structured for this flow: `master` carries no changes, and the
`alpine` branch keeps its diff concentrated in the entry-point files
(builtin registry, settings gating, one platform file, build/deploy script,
CI, README).

## Tests

```sh
sh projects/APPLaunch/tests/run_tests.sh        # C++ + python contracts
sh ext_components/cp0_lvgl/tests/run_tests.sh   # platform layer (85 tests)
```

The python contract tests encode this fork's shape too (no Store/Python
entries, the Alpine gating in the settings tree).

## License

MIT for M5Stack's launcher code (see [LICENSE](LICENSE)); third-party
components keep their upstream licenses ([LICENSES/](LICENSES/),
[ext_components/THIRD_PARTY_NOTICES.md](ext_components/THIRD_PARTY_NOTICES.md)).
