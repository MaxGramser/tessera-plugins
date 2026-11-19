# Changelog

## 0.5.0 - 2026-11-19
- Timers by voice: "set a timer for 5 minutes", "how long is left on the timer", "cancel the timer". They are Home Assistant's own timers; the screen now tells Home Assistant it can hold them (ESPHome's on_timer triggers).
- What is left of the timer that ends first shows in the top bar (a new item, Timer) and on the assistant's tile, with a pause sign while it waits.
- A timer that runs out shows a popup with its name and rings (components/voice_assist/sounds/timer.wav) until a tap on it or the wake phrase, at most 5 minutes; the screen stays awake while it rings. A timer that runs out during a question waits until the answer was spoken.
- A new setting, Timer alarm, turns the popup and the ring off.
- German texts on the screen and in the editor (translations/de.json), and the timers' texts in Dutch.

## 0.4.0 - 2026-10-25
- The voice assistant takes its turn with the screen's other sounds. A doorbell's ring stops the wake word while someone is at the door and lets it listen again after; a question pauses music that plays, and the music stays paused after the answer, as it does for a ring.
- Needs Tessera with plugin API 0.19 (dev); update the screen with it.

## 0.3.0 - 2026-10-20
- A popup over the screen while it listens, thinks and answers, also when the screen was dark or asleep: rings that ripple while it listens, an arc that turns while it thinks, what it heard and the answer word by word as it is spoken. It stays 30 seconds after the answer, with a close key. A new setting, Show popup, turns it off.
- The thinking sound stops as soon as a long answer starts to play, instead of going on over its first words.
- Needs Tessera with plugin API 0.18.

## 0.2.3 - 2026-10-10
- A new wake sound.

## 0.2.2 - 2026-10-10
- New wake and thinking sounds. The thinking sound plays again as soon as it ends, so its own length sets the rhythm.

## 0.2.1 - 2026-10-10
- Its sounds play through Tessera's own sound player; on a screen whose microphone and speaker share one bus, a sound that cannot play while it listens is left out.

## 0.2.0 - 2026-10-10
- A short sound when the screen starts to listen, and a soft plop again and again while Assist thinks, each with a setting to turn it off (Wake sound, Thinking sound).
- In the top bar and on the tile, a microphone that lights up while it listens, dots in amber while it thinks and a speaker while it answers; the top bar item can show its icon alone.
- Needs the screen's speaker as well.

## 0.1.0 - 2026-10-10
- First version: Home Assistant's Assist on the screen, with a wake phrase recognised on the screen, a tile, a top bar item and two settings.
