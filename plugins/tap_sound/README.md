# Tap sound

A short, soft tick on the screen's speaker for every tap the screen takes: a tile, a key, a row of the settings, the
top bar. It plays at the screen's volume, so turn the screen down and the tick is quieter too.

## What you need

- A screen with a speaker: a board that brings one (the reTerminal D1001, the Waveshare ESP32-P4-86 panel) or a plugin
  that does.

## Set up

1. In Tessera, open **Plugins**, choose **Tap sound** and tick your screens. Each screen builds once with it.
2. That is all: the screen ticks on every tap from then on.

## On the screen

Hold the top bar, then **Settings**, **Plugins**, **Tap sound**: **Tap sound** turns the tick off and on. The same switch
is an entity of the screen in Home Assistant ("Tap sound") and in the plugin's details on the screen's Plugins tab. The
screen's **Volume** is under **Extras**.

## The sound

It is one file, `components/tap_sound/sounds/tap.wav`: a WAV file of 16-bit samples, mono, at 48 kHz. A plugin of your
own made from this one can swap it; the build says so when a file is not such a WAV. On a screen whose microphone and
speaker share one bus (the Waveshare panel), a tick waits while something listens and is left out when that takes
longer than a second.
