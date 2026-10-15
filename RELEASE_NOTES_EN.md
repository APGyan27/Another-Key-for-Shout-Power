# Another Key for Shout (AKFS) — Release Notes 1.0.2

Target runtime: Skyrim SE 1.6.1170 (AE) · CommonLibSSE-NG + Address Library

## Release 1.0.2 (internal v1.5) — 2026-10-14

### 5. Shouts can no longer get stuck after prolonged use (stale voice-cooldown self-heal)
- Symptom: after using for a while, X stops releasing any shout entirely (the cooldown gate was silently blocking, no log/UI).
- Cause: the engine **never writes `voiceRecoveryTime`** for X-injected shouts (logs: stays 0.00s before/after every cast).
  `OnCooldown`'s shout branch keyed off `remain>0` — a value the engine won't clear on its own. Once some async cast left a stale `voiceRecoveryTime>0`, the shout slot locked permanently (cumulative → "after prolonged use").
- Fix: in `OnCooldown`'s shout branch, treat a stale `voiceRecoveryTime>0` as garbage and **clear `high->voiceRecoveryTime` + allow the cast** whenever the plugin is not actively casting (`!m_shoutActive && !m_restorePending`). Only intercept when actively casting. Clears `recoveryTime` only — never writes `voiceState`.

## Release 1.0.1 (internal v1.4) — 2026-10-14

### 1. First X press after setting the slot no longer fires level-3 shout
- Cause: the slot-setting X release set `m_storePendingRelease=true`; the release was swallowed and
  never reached `FinishNativeShout` (which starts a real cast on the down edge), leaking `m_shoutActive`
  so the next X accumulated the previous wall-clock hold → level 3.
- Fix: swallow the store-pending release only when a real cast hasn't started;
  clear `m_storePendingRelease` before `OnXDown()` outside menus.

### 2. "Once per game day" powers no longer stuck the next day
- Cause: `PowerUsedToday` returned "entry exists" rather than "stored day == current day"; cross-day
  cleanup only ran on a successful cast, so a blocked entry never cleared.
- Fix: compare `int64(stored day) == int64(current GetDaysPassed())` (uses `RE::Calendar`).

### 3. Loading an older/smaller-day save never false-blocks
- Since fix #2 compares values, loading an older day, restarting, etc. can only be *more lenient*
  (treated as unused) — never blocked.

### 4. Once-per-day only for kPower; repeatable powers & specials get a backdoor
- Previous build gated *all* powers → wrongly blocked repeatable `kLesserPower` (Vampire Lord, Night Eye,
  Black Market, ...). UESP: Greater Powers(kPower) = once/day, Lesser Powers = any number.
- Now: `IsDailyPower()` = `{SpellType==kPower} && !DailyBypassContains(formID)`.
  `kLesserPower(3)/kVoicePower(11)` always use only the 3s short cooldown.
- Backdoor: ini `[Settings] DailyBypass=<formID1>,<formID2>…` (8-digit hex, comma/space separated).

## Config (AKFS.ini)
```
[Settings]
KeyCode=45            ; trigger key DIK scancode (default X)
PowerOncePerDay=1     ; kPower once per game day (default on), 0=off → only 3s short cooldown
DailyBypass=          ; kPower formIDs exempt from once/day (comma-separated 8-digit hex)
                      ;   e.g. repeatable Beast Form → DailyBypass=00092C48
[Lang]
Language=English      ; or 中文
```

## Install
- Release ZIP: `Another Key for Shout 1.0.1.zip` — FOMOD (Mod manager friendly), DLL + language ini.
- Manual: copy `AKFS.dll` to `Data\SKSE\Plugins\AKFS.dll`, put `AKFS.ini` next to it.
- Requires Address Library for SKSE Plugins (SE 1.6.x) matching runtime 1.6.1170.
- Logs: `My Games\Skyrim Special Edition\SKSE\Another Key for Shout.log`.