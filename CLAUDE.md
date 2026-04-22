# Premonition — project instructions

## Building the plugin

When the user asks to "build the plugin" (or similar), build all viable macOS targets in one pass using the Xcode workspace. Skip AAX (SDK not present) and AUv3 family (needs real signing). Build unsigned so no provisioning profile is required.

```bash
for scheme in macOS-VST3 macOS-AUv2 macOS-APP macOS-CLAP; do
  xcodebuild -workspace Premonition/Premonition.xcworkspace \
    -scheme "$scheme" -configuration Debug build \
    CODE_SIGN_IDENTITY="-" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO
done
```

Report per-scheme pass/fail at the end. Do NOT attempt these unless explicitly asked:
- `All macOS`, `macOS-AAX` — AAX SDK not vendored (`iPlug2/Dependencies/IPlug/AAX_SDK/` missing)
- `macOS-AUv3`, `macOS-AUv3Framework`, `macOS-APP with AUv3` — require Mac Development cert for team `686EDA2T8T` (AUv3 can't be built unsigned)
- `macOS-VST2` — VST2 SDK (`aeffectx.h`) not vendored; Steinberg discontinued distribution

Build artifacts install to `~/Library/Audio/Plug-Ins/{VST3,VST,Components}/`.

## Handoff docs

Files live in `handoff/`. Treat `HANDOFF-next.md` and the most recent dated `HANDOFF-YYYY-MM-DD-*.md` as the live state. At the end of a session (or whenever work on an item wraps), update the handoff:
- Append a "Session accomplishments" entry to the latest handoff, or create a new dated handoff.
- Mark old handoff docs as completed **only if their items are actually done** — verify before marking. Rename done ones to `HANDOFF-YYYY-MM-DD-*-DONE.md`, or move them to `handoff/archive/`. Never mark a doc done based on assumption; ask or check the code first.
- Keep `HANDOFF-next.md` pointing at the current priority list.
