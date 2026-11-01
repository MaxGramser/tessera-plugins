"""The OmniVision OV5647 (the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B's camera kit, and the Raspberry Pi cameras of that
sensor): its colours in the order esp_cam_sensor declares, however the camera flips it.

Espressif's esp_cam_sensor drives this sensor, and its formats say its colours come in the order GBRG. They do only with
both of its mirror bits set (0x3821 0x07, as Linux's driver starts it), where the formats set one (0x03): the picture
came out purple, with orange as green. And a flip, which is how Screen camera turns the picture (picture_turn.h), moves
the readout by a row or a column and the colours with it: turned half way round, red and blue swapped. The image
processor reads the order once, from the format, so the sensor keeps to it instead: this component sets the second
mirror bit and moves the ISP's window by a pixel for each flip, every time the camera has handed the sensor its controls
(esp_video_camera's camera_sensor.h, controls_set): after every start, which hands the sensor its format again, and
after a turn. A sensor of another id is left alone. ov5647_window.h says the numbers and where they were measured.
"""
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c", "esp_video_camera"]

ov5647_ns = cg.esphome_ns.namespace("ov5647")
OV5647 = ov5647_ns.class_("OV5647", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(OV5647)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x36))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
