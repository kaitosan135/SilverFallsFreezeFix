DISCLAIMER: Made with Grok AI.

# SilverFallsFreezeFix

Aroma/WUPS plugin that acts as a failsafe for Silver Falls games on Wii U.

When a Silver Falls game soft-freezes (visuals frozen, audio continues, system locked) and HaltFix does not trigger (because `PPCHalt` is never called), this plugin detects the lack of frame presentation and reboots the console.

## Supported titles
- `000500001021D000` – Undertakers
- `000500001021D300` – White Inside Its Umbra
- `000500001021DD00` – Ghoul Busters

## How it works
- Hooks `GX2SwapScanBuffers` and updates a timestamp on every frame.
- Runs a high-priority watchdog thread.
- After a 30-second grace period (for loading), if no frame swap occurs for 10 seconds, it reboots using the same iosuhax method as HaltFix.

## Build requirements
- devkitPPC
- wut
- WiiUPluginSystem (WUPS)
- libmocha
- libkernel

## Build
```bash
make
```
Or with logging:
```bash
make DEBUG=1
```

The output is `SilverFallsFreezeFix.wps`.

## Install
Copy `SilverFallsFreezeFix.wps` to:
```
sd:/wiiu/environments/aroma/plugins/
```
Reboot into Aroma. The plugin only activates for the three Silver Falls title IDs listed above.

## Tuning
Edit the constants in `src/main.cpp`:
- `timeoutTicks` (default 10 seconds of no frames)
- `graceTicks` (default 30 seconds after app start)

## Notes
This is a pure failsafe. It does not interfere with other games. If a freeze is so severe that even the watchdog thread stops, a forced power cycle is still required (rare for the “visuals frozen, audio running” case).
