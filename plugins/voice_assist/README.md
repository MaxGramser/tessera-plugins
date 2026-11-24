# Voice assistant

Talk to Home Assistant's Assist from the screen. The screen listens for a wake phrase on its own, and only then sends
what you say to Home Assistant; the answer comes from the screen's speaker.

Speech to text, the conversation and text to speech are whatever your Home Assistant uses for Assist: its own
conversation agent with Whisper and Piper, Home Assistant Cloud, or an AI such as OpenAI or Claude. The screen is a
voice satellite in Home Assistant, in the area you give it.

## What you need

- A screen with a microphone and a speaker: a board that brings them (the reTerminal D1001) or a plugin that does.
- An Assist pipeline in Home Assistant (Settings, Voice assistants) with speech to text and text to speech.

## Set up

1. In Tessera, open **Plugins**, choose **Voice assistant** and tick your screens. Each screen builds once with it.
2. In Home Assistant, open the screen's device (Settings, Devices) and choose its Assist pipeline under
   **Assist pipeline**, if it is not the preferred one.
3. Put the tile **Assistant** on a page: it says what the assistant does, and a tap asks a question without the wake
   phrase, or stops one.
4. Optional: add **Assistant** to a page's top bar (Top bar, Add, From plugins). It shows while the assistant listens,
   thinks or answers.

## On the screen

Hold the top bar, then **Settings**, **Plugins**, **Voice assistant**:

- **Wake word**: whether the screen listens for its wake phrase. Off, a tap on the tile still asks.
- **Wake phrase**: Okay Nabu, Hey Jarvis or Hey Mycroft.
- **Wake sound**: a short sound when the screen starts to listen, after the wake phrase or a tap.
- **Thinking sound**: a soft sound, again and again, while Home Assistant works out the answer.
- **Show popup**: a popup over the screen while it listens, thinks and answers (on unless you turn it off). See below.
- **Timer alarm**: a popup and a ring when a timer set by voice runs out (on unless you turn it off). See Timers.
- **Ask now**: asks a question at once.

The settings are entities of the screen in Home Assistant too ("Wake word", "Wake phrase", "Wake sound", "Thinking
sound", "Show popup", "Timer alarm"), and the editor shows them in the plugin's details on the screen's Plugins tab. The screen's own **Microphone** switch, on a board that has one,
silences it for the assistant as well.

## The popup

As soon as the screen hears its wake phrase, or you tap the tile, a popup comes over the screen, also when the screen
was dark or asleep. Rings ripple out of the microphone while it listens, an arc turns while Home Assistant thinks, and
under them comes what it heard and then the answer, word by word as the speaker says it; a long answer shows its last
lines. The popup stays 30 seconds after the answer so you can read it again. Its close key, a tap on it or a tap beside
it closes it sooner; while a question goes on, a tap on the popup stops it. It needs Tessera with plugin API 0.18.

The words of the answer come all at once from Home Assistant, at the end of its answer: Home Assistant does not send
an answer in pieces to an ESPHome screen while it is being written. The popup shows them at the pace of speech, in
step with the speaker.

## Timers

Say "set a timer for 5 minutes", "set a pizza timer for 10 minutes", "how long is left on the timer", "add 2 minutes to
the timer" or "cancel the timer". Timers are Home Assistant's own; the screen tells Home Assistant it can hold them, and
Home Assistant tells it when one starts, changes and runs out. They are Assist's timers, set by voice, not Home
Assistant's timer helpers (`timer.*` entities): those show with the Timers plugin.

- **Top bar**: add **Voice timer** to a page's top bar in the editor (**Edit top bar**, **+**, **From plugins**). It shows what is left of the timer that
  ends first ("4:59", "Pizza 4:59", "+1" for one more), a pause sign while all wait, and a red bell while one rings.
- **Tile**: the assistant's tile shows the same while no question goes on.
- **When it runs out**: a popup with the timer's name comes over the screen, also when it was dark, and the timer sound
  rings again and again. A tap on the popup, or the wake phrase, stops it; it rings at most 5 minutes, and the screen
  stays awake while it rings. A timer that runs out during a question waits until the answer was spoken, and through
  the follow-up when Assist asks back.
- **Music** on the screen pauses while it rings, and stays paused after, as it does for a question.
- **A doorbell** takes the screen over: its alert closes the timer's popup, and the ring stops with it.
- **Timer alarm** (a setting, on unless you turn it off): the popup and the ring. Off, timers still run in Home
  Assistant, silently.

A timer belongs to the screen it was set on: ask that screen how long is left.

## The sounds

They are three files in `components/voice_assist/sounds/`: `wake.wav`, `thinking.wav` (the screen plays it again as
soon as it ends, so its length sets the rhythm) and `timer.wav` (again every 1.5 seconds while a timer rings). Each is a WAV file of 16-bit samples, mono, at 48 kHz like the screen's speaker. A plugin of your
own made from this one can swap them; the build says so when a file is not such a WAV.

## Privacy

The wake phrase is recognised on the screen itself; nothing leaves the screen until it is heard or you tap the tile.
After that the question goes to your Home Assistant and on to whatever speech and conversation services its pipeline
uses.
