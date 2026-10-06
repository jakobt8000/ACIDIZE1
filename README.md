# ACIDIZE

Acid bass synthesizer. A product of REZONANZA.

VST3 + AU for Mac (Ableton). Window: 1020 × 516 px with the piano roll open, 1020 × 195 px closed. Mono voice with saw/square/sub, resonant low-pass filter with envelope,
accent, slide, drive and swing. Built-in 16-step sequencer with a piano roll (C2 to C4) and scale lock.

## Build (GitHub)

1. Create a new repo on GitHub and upload everything in this folder (including the hidden `.github` folder).
2. GitHub builds the plugin automatically on every push (tab **Actions** › *Build macOS*).
3. When the run is green, download **ACIDIZE-mac** under *Artifacts* and unzip it.

## Install

Copy the two files to the system plug-in folders:

```
sudo cp -R ACIDIZE.vst3 /Library/Audio/Plug-Ins/VST3/
sudo cp -R ACIDIZE.component /Library/Audio/Plug-Ins/Components/
sudo xattr -cr /Library/Audio/Plug-Ins/VST3/ACIDIZE.vst3
sudo xattr -cr /Library/Audio/Plug-Ins/Components/ACIDIZE.component
```

Restart Ableton (rescan plug-ins if it doesn't show up). ACIDIZE is under *Plug-Ins › REZONANZA*.

## Using it

- **PLAY** starts the internal sequencer. It follows Ableton's tempo, and locks to the timeline while Ableton is playing.
- With the sequencer stopped, ACIDIZE plays from MIDI (velocity above 100 = accent, overlapping notes = slide).
- **Piano roll**: click an empty cell to add a note, click a note to remove it, drag up/down to change pitch.
  `A` and `S` under each step toggle accent and slide.
- **SCALE**: choose root and scale; notes outside the scale are greyed out and can't be used.
- **RND PATTERN** makes a new random pattern inside the chosen scale.
- Double-click a knob to reset it. The preset arrows load the 5 built-in sounds with their patterns.

## Files

- `Source/PluginProcessor.*` – sound engine, sequencer, presets, state
- `Source/PluginEditor.*` – UI (knobs, BARS display, piano roll, info window)
- `Resources/` – ACIDIZE logo and IBM Plex Mono (SIL Open Font License, see `OFL.txt`)
