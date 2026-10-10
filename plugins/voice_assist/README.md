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
- **Ask now**: asks a question at once.

Both settings are entities of the screen in Home Assistant too ("Wake word", "Wake phrase"), and the editor shows them
in the plugin's details on the screen's Plugins tab. The screen's own **Microphone** switch, on a board that has one,
silences it for the assistant as well.

## Privacy

The wake phrase is recognised on the screen itself; nothing leaves the screen until it is heard or you tap the tile.
After that the question goes to your Home Assistant and on to whatever speech and conversation services its pipeline
uses.
