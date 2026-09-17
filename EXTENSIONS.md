# Extensions

Extra features this fork adds on top of
[OniARM64](https://github.com/andiyar/OniARM64), the native Apple Silicon port
of Bungie's *Oni* (2001).

The port is the base; everything here is layered on top of it and keeps
upstream's behaviour wherever it can. One section per extension, ordered as
they landed. Each one covers how to use it and what it does and does not do,
with a short **For maintainers** note at the end giving the code layout and
the limits worth knowing before you change anything.

Player-facing announcements live in [CHANGELOG.md](CHANGELOG.md); the
per-commit developer detail is in [HISTORY.md](HISTORY.md).

---

## Quick save and quick load

Save and reload anywhere in a level, without walking back to a save point.

Story save points are placed at chapter boundaries by the level designer, and
that is still how campaign progress is recorded. Quick save covers the other
case: you are twenty minutes into a firefight, it is about to go badly, and
you want to keep what you have.

### Using it

**Data Pad.** Open the pad with **F1**. **Quick Save** and **Quick Load** sit
in the left column, under **Help**. Press either one and the pad closes and
the work happens — there is no confirmation step. **Quick Load** is greyed out
until a quick save exists, so it cannot do anything surprising on a fresh
install.

**Keys.** **F5** saves, **F9** loads. A fresh install's `key_config.txt` gets
both. If you already have one, add them:

```
bind fkey5 to quick_save
bind fkey9 to quick_load
```

Neither is behind the developer-access gate, so you do not need cheats on.

**Script.** Level scripts and the developer console get `quick_save` and
`quick_load`.

Quick save needs a loaded level and a player character, so pressing it from
the main menu does nothing but say **Cannot save here**.

### What a quick load puts back

The level is rebuilt from its own data at the chapter you were in — the state
its designer placed — and then your changes are applied on top:

- **You** — health, weapons, position and facing.
- **Enemies** — each one's position, facing and current hit points. A guard
  you had whittled to half health comes back at half health.
- **Bodies** — enemies you killed come back as corpses, where they fell and in
  the pose they landed in.
- **Doors** — doors you opened or unlocked come back that way. A door you
  never touched comes back as the designer left it, locked if it was locked.
- **Glass** — panes you broke stay broken.

### What it does not put back

A quick save snapshots the world, not time.

- **Scripts re-run.** Anything a script already did — a trigger you crossed, a
  door a script opened, a spawn wave — happens again on load.
- **AI starts over.** Enemies reassess from scratch and will not remember they
  were flanking you.
- **The clock resets.** The level's elapsed time starts again from zero.

This is inherent to how the reload works: the level is genuinely rebuilt, and
only the pieces listed above are carried across.

### Your saved games are not touched

Quick save writes its own file and never goes near `persist.dat`. Save points,
unlocked levels and diary pages are untouched, and a quick load cannot cost
you a real save.

### The file

One file, one slot:

```
~/Library/Application Support/OniARM64/quick_save.dat
```

A second quick save overwrites the first. If a `quick_save.dat` already exists
in the directory you launch the game from, that copy is used instead — handy
when you are running a build straight out of the repository.

Delete the file and **Quick Load** greys out again the next time you open the
pad.

### What you will see

| Message | When |
| --- | --- |
| *Game saved* | Quick save succeeded, with the game's save chime. |
| *No quick save* | Quick load found nothing, or the file was unreadable. |
| *Cannot save here* | Quick save with no level loaded or no player. |
| *Could not write quick save* | The file could not be written. |

The chime plays at full volume from the Data Pad buttons. The pad fades the
game's audio down while it is open, and both buttons wait until the pad is
gone before doing anything, so the chime is not caught in that fade.

### For maintainers

`Oni_QuickSave.c/.h` owns the file format, `Oni_GameState.c` captures and
applies, `Oni_InGameUI.c` owns the Data Pad buttons.

**Format.** A fixed header — level, save point, player record, four record
counts — followed by four variable-length runs: characters, doors, geometry
quads, corpses. Only live records are written, so the file stays proportional
to its contents rather than to the array maxima.

Every record is fixed-size and pointer-free *by construction*, because each
one has to survive `ONrLevel_Unload`. Identity is a name, an ordinal or an ID,
never an address. Two consequences worth knowing before extending it:

- Character identity is `(name, ordinal)`, not name alone. Shipped levels
  spawn the same name repeatedly on purpose — `respawn1` up to three times in
  `power`, `WH_Striker_C` four times in `EnvWarehouse` — so a name is not a
  key.
- A corpse record stores its character class as a *name*. `ONtCorpse_Data`
  holds an `ONtCharacterClass *` that dies with template memory and the corpse
  renderer dereferences it, so restoring a corpse by byte copy would be a
  use-after-free.

**The reader rejects, it does not repair.** Missing, empty, truncated, short,
wrong version, wrong swap code, and counts that would overrun the fixed arrays
all end in a clean "no quick save". The count guard is the load-bearing one: a
count read off disk indexes a fixed array.

Adding a field means bumping `ONcQuickSave_Version`; the swap code is a second
guard against a file written by a different build.

**Both Data Pad buttons defer.** Each one closes the pad and sets a pending
flag, and `Oni.c` consumes it on the same iteration once the pad is destroyed.
The deferral is not load-bearing for safety — the modal loop has the world
frozen, and every capture is a read-only walk — it exists so the save chime
plays at full volume instead of the pad's 50% cut, and so both buttons have
one shape.

**The buttons reuse controls 112/113**, Bungie's multi-page-help Previous/Next
buttons. They are present and art-complete in the binary dialog template but
have been dead since that feature was cut, and the template is game data that
cannot be edited from source — so reusing them is the only route that does not
mean building controls at runtime. They land at `217,408` and `217,468`, one
and two rows below **Help**, with the row pitch read from the tab column
itself rather than hardcoded.

**Tests** are standalone and need no framework:

```
cc -Wall -Wextra -DUUmSDL=1 \
   -IBungieFrameWork/BFW_Headers -IOniProj/OniGameSource \
   -I/opt/homebrew/include \
   tests/test_oni_quick_save.c OniProj/OniGameSource/Oni_QuickSave.c \
   -o /tmp/test_oni_quick_save
/tmp/test_oni_quick_save
```

**Known gap:** the Data Pad buttons were verified headlessly, by posting the
same window-manager message a keypress produces. That is not a mouse. Hit
testing, focus behaviour and the on-screen look of the reused art against the
partspec all still want a human at the keyboard.
