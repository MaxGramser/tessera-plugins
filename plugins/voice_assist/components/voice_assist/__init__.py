"""Voice assistant: Home Assistant's Assist on the screen, its state on a tile and in the top bar.

The wake word (micro_wake_word), the Assist pipeline (voice_assistant) and the settings are ESPHome components of
plugin.yaml; this component shows what they do, takes a tap, and plays its two sounds on the screen's speaker
(ts_speaker): sounds/wake.wav when it listens, sounds/thinking.wav again and again while Assist thinks. A sound is a
WAV file of 16-bit samples, mono or stereo, best at the screen's own rate (48 kHz); swap the file to change it.
register_plugin() takes the id, version and the screen's texts from the manifest and translations/ beside this folder.
"""
from pathlib import Path
import wave

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import micro_wake_word, select, smart_display, speaker, switch, voice_assistant
from esphome.const import CONF_ID
from esphome.core import ID, HexInt

DEPENDENCIES = ["smart_display", "voice_assistant", "micro_wake_word"]

CONF_ASSISTANT = "assistant"
CONF_WAKE_WORD = "wake_word"
CONF_WAKE_WORD_ON = "wake_word_on"
CONF_WAKE_PHRASE = "wake_phrase"
CONF_PHRASES = "phrases"
CONF_SPEAKER = "speaker"
CONF_WAKE_SOUND_ON = "wake_sound_on"
CONF_THINKING_SOUND_ON = "thinking_sound_on"

SOUNDS = Path(__file__).parent / "sounds"
SOUND_NAMES = ("wake", "thinking")


def read_sound(name):
    """(rate, channels, samples) of sounds/<name>.wav, or the reason it cannot be played."""
    path = SOUNDS / f"{name}.wav"
    try:
        with wave.open(str(path), "rb") as file:
            if file.getcomptype() != "NONE" or file.getsampwidth() != 2 or file.getnchannels() not in (1, 2):
                raise cv.Invalid(f"voice_assist: sounds/{path.name} must be a WAV of 16-bit samples, mono or stereo")
            return file.getframerate(), file.getnchannels(), file.readframes(file.getnframes())
    except (OSError, EOFError, wave.Error) as error:
        raise cv.Invalid(f"voice_assist: sounds/{path.name}: {error}") from error


def check_sounds(config):
    for name in SOUND_NAMES:
        read_sound(name)
    return config

voice_assist_ns = cg.esphome_ns.namespace("voice_assist")
VoiceAssist = voice_assist_ns.class_("VoiceAssist", cg.Component)

CONFIG_SCHEMA = cv.All(cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VoiceAssist),
        cv.Required(CONF_ASSISTANT): cv.use_id(voice_assistant.VoiceAssistant),
        cv.Required(CONF_WAKE_WORD): cv.use_id(micro_wake_word.MicroWakeWord),
        cv.Required(CONF_WAKE_WORD_ON): cv.use_id(switch.Switch),
        cv.Required(CONF_WAKE_PHRASE): cv.use_id(select.Select),
        # The wake word models in the order of Wake phrase's options.
        cv.Required(CONF_PHRASES): cv.All(cv.ensure_list(cv.use_id(micro_wake_word.WakeWordModel)), cv.Length(min=1)),
        # The speaker its sounds play on, and the two settings that turn them off.
        cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
        cv.Required(CONF_WAKE_SOUND_ON): cv.use_id(switch.Switch),
        cv.Required(CONF_THINKING_SOUND_ON): cv.use_id(switch.Switch),
    }
).extend(cv.COMPONENT_SCHEMA), check_sounds)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_assistant(await cg.get_variable(config[CONF_ASSISTANT])))
    cg.add(var.set_wake_word(await cg.get_variable(config[CONF_WAKE_WORD])))
    cg.add(var.set_wake_word_on(await cg.get_variable(config[CONF_WAKE_WORD_ON])))
    cg.add(var.set_wake_phrase(await cg.get_variable(config[CONF_WAKE_PHRASE])))
    for phrase in config[CONF_PHRASES]:
        cg.add(var.add_phrase(await cg.get_variable(phrase)))
    cg.add(var.set_speaker(await cg.get_variable(config[CONF_SPEAKER])))
    cg.add(var.set_wake_sound_on(await cg.get_variable(config[CONF_WAKE_SOUND_ON])))
    cg.add(var.set_thinking_sound_on(await cg.get_variable(config[CONF_THINKING_SOUND_ON])))
    # The sounds in flash, as the samples of their WAV files.
    for name in SOUND_NAMES:
        rate, channels, samples = read_sound(name)
        data = cg.progmem_array(ID(f"{config[CONF_ID]}_{name}_sound", is_declaration=True, type=cg.uint8),
                                [HexInt(b) for b in samples])
        cg.add(var.set_sound(name == "thinking", data, len(samples), rate, channels))
