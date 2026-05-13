# rcheevos PAL hash-mismatch — investigation findings

Date: 2026-05-13
Branches at investigation time:
- `pcsx2-libretro@retronest-libretro` HEAD `eece07804`
- `RetroNest-Project@main` HEAD `699870d`

Resolves the open question carried into this session by
`memory/rcheevos_pal_hash_mismatch.md`.

## TL;DR — Case B: RA database coverage gap, not a bug on our side

For DBZ Budokai Tenkaichi 2 (PAL) and Looney Tunes Space Race (PAL),
RetroAchievements only has the **USA** disc hash registered. The PAL
hash isn't in RA's index, so `rc_client_begin_identify_and_load_game`
correctly returns "Unknown game". `rc_hash_ps2` is functioning correctly
— Wacky Races (PAL) hashes successfully and resolves to GameID 25968
because RA's curators happened to register the European disc hash for
that title.

No code change in pcsx2-libretro or RetroNest will fix this. The fix is
either (a) submit the missing PAL disc hashes to RA's database, or (b)
accept the coverage gap.

## Evidence

### Hash recipe (rcheevos 12.3.0, `src/rhash/hash_disc.c:982 rc_hash_ps2`)

```
exe_name  = boot file name from SYSTEM.CNF BOOT2= line, with
            "cdrom0:" prefix and leading '\\' stripped, truncated at ';'
            (e.g., "SLES_541.64")
hash      = MD5(exe_name || full ELF contents of <exe_name>)
```

Because `exe_name` is the disc serial string and PAL/USA versions of the
same title use different serials by design (`SLES_xxx.xx` vs `SLUS_xxx.xx`),
PAL and USA dumps of the same game **never** produce the same hash. RA
must register both region hashes against the same GameID for both regions
to be identifiable.

### Local hash computations

Tool: `pcsx2-libretro/tools/test_rcheevos_hash.c` — links against
RetroNest's pre-built `librcheevos_static.a` and calls
`rc_hash_generate_from_file(hash, RC_CONSOLE_PLAYSTATION_2, path)`.

| Disc | Local hash (rcheevos 12.3.0) |
|------|------------------------------|
| R&C 2 NTSC (SCUS-97268) | `8b2100df5d372fc69be6f41ba4c70de7` |
| Wacky Races PAL (SLES-50183) | `ad9fa05b7e9817730e062e3cada64532` |
| Looney Tunes Space Race PAL (SLES-50487) | `5bfe82ab2b1f0449ad83766f43b75d0d` |
| DBZ Budokai TT2 PAL (SLES-54164) | `fd54aebfcd92cc993c2fcc5d763fb98f` |

### RA `dorequest.php?r=gameid&m=<hash>` resolution

(Identical endpoint rcheevos calls internally; UA = `RetroNest/1.0.0 rcheevos/12.3`.)

| Hash | RA GameID | Notes |
|------|-----------|-------|
| `8b21…0de7` (R&C 2 NTSC) | **3072** | matches log line from SP6 smoke |
| `ad9f…4532` (Wacky Races PAL) | **25968** | RA registered the European hash |
| `5bfe…5d0d` (Looney Tunes PAL) | **0** | not in RA index |
| `fd54…b98f` (DBZ TT2 PAL) | **0** | not in RA index |

### What RA actually has registered for the failing games

`API_GetGameHashes.php?i=<game_id>`:

```
GameID 20588 — Dragon Ball Z: Budokai Tenkaichi 2
  653c981e5474d286a0dd75c1310b998d  "Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja)" [redump]
  (only entry — no PAL hash registered)

GameID 21882 — Looney Tunes: Space Race
  70fa62084bbfa1bf067468ee934c325d  "Looney Tunes - Space Race (USA) (En,Fr,Es)" [redump]
  (only entry — no PAL hash registered)

GameID 25968 — Wacky Races
  ad9fa05b7e9817730e062e3cada64532  "Wacky Races Starring Dastardly & Muttley (Europe) (En,Fr,De,Es,It,Nl)" [redump]
  (only entry — and it's the European disc, hence works for our user)
```

RA's catalog has **one** GameID per title regardless of region; multiple
region dumps are listed under the same `API_GetGameHashes` response. So
the absence of a PAL line means no PAL hash exists, not "PAL is under a
different GameID we missed".

## Why this rules out the other two cases

- **Case A** (RA has PAL hash, ours differs): impossible — RA has no PAL
  hash registered for these two games at all.
- **Case C** (rc_hash_ps2 cdreader bug for PAL): impossible — Wacky
  Races (PAL, SLES-50183, .bin format) hashes correctly to RA's
  registered PAL hash. Same code path runs for all four discs.

## Recommendation

1. **Don't write client-side workarounds.** Cross-region matching by
   serial+name would require RetroNest to claim the user has a US disc
   when they have a PAL one. RA's achievement triggers reference
   specific memory addresses in the **US ELF binary**; on a PAL ELF
   those addresses likely map to different code/data, so cheevos would
   misfire (false unlocks or never-trigger).
2. **Submit the missing PAL hashes to RA**, if the user wants these
   specific titles supported. Process: open a hash-add request on RA
   Discord (#hash-help) or the relevant game's forum thread, providing
   the hash + Redump-source disc dump info. A developer of that PS2
   set will validate and merge. This is the right layer — fixes it
   for every RA user, not just RetroNest.
3. **Accept the coverage gap as expected behavior** for any PAL PS2
   disc whose hash isn't yet in RA. The current "Unknown game" log line
   is correct; the only enhancement worth considering on our side is
   rephrasing the log message so it doesn't read like a bug. Optional,
   low priority.

## Investigation tool

Lives at `pcsx2-libretro/tools/test_rcheevos_hash.c`. Manual-compile, no
PCSX2 link required. Recipe in the file's header comment. Reuse for any
future "is hash X in RA's index?" question.
