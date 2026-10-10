"""Screen camera: the camera in the top bar while Home Assistant takes pictures of the screen's own camera.

The camera itself is esp_video_camera beside this folder (plugin.yaml); this component listens to it, as ESPHome's API
does, and says on the screen when pictures leave it. register_plugin() takes the id, version and the screen's texts
from the manifest and translations/ beside this folder.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display", "camera"]

CONF_CAMERA = "camera"

screen_camera_ns = cg.esphome_ns.namespace("screen_camera")
# ESPHome's camera base (esphome/components/camera/camera.h), which declares no class of its own in Python.
Camera = cg.esphome_ns.namespace("camera").class_("Camera", cg.EntityBase, cg.Component)
ScreenCamera = screen_camera_ns.class_("ScreenCamera", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ScreenCamera),
        cv.Required(CONF_CAMERA): cv.use_id(Camera),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
    cg.add(var.set_camera(await cg.get_variable(config[CONF_CAMERA])))
