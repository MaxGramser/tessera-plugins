"""Voice assistant: Home Assistant's Assist on the screen, its state on a tile and in the top bar.

The wake word (micro_wake_word), the Assist pipeline (voice_assistant) and the two settings are ESPHome components of
plugin.yaml; this component shows what they do and takes a tap. register_plugin() takes the id, version and the
screen's texts from the manifest and translations/ beside this folder.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import micro_wake_word, select, smart_display, switch, voice_assistant
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display", "voice_assistant", "micro_wake_word"]

CONF_ASSISTANT = "assistant"
CONF_WAKE_WORD = "wake_word"
CONF_WAKE_WORD_ON = "wake_word_on"
CONF_WAKE_PHRASE = "wake_phrase"
CONF_PHRASES = "phrases"

voice_assist_ns = cg.esphome_ns.namespace("voice_assist")
VoiceAssist = voice_assist_ns.class_("VoiceAssist", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VoiceAssist),
        cv.Required(CONF_ASSISTANT): cv.use_id(voice_assistant.VoiceAssistant),
        cv.Required(CONF_WAKE_WORD): cv.use_id(micro_wake_word.MicroWakeWord),
        cv.Required(CONF_WAKE_WORD_ON): cv.use_id(switch.Switch),
        cv.Required(CONF_WAKE_PHRASE): cv.use_id(select.Select),
        # The wake word models in the order of Wake phrase's options.
        cv.Required(CONF_PHRASES): cv.All(cv.ensure_list(cv.use_id(micro_wake_word.WakeWordModel)), cv.Length(min=1)),
    }
).extend(cv.COMPONENT_SCHEMA)


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
