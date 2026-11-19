package launcher

import storage.Storage
import system.System
import ui.Ui

class ShadeScreen {
    fun panel(sound: String, vibrate: String) {
        System.time("clock")
        System.deviceInfo("device")
        System.memory("memory")
        if (sound == "1") {
            Ui.setText("sound", "Sound: On")
        } else {
            Ui.setText("sound", "Sound: Off")
        }
        if (vibrate == "1") {
            Ui.setText("vibrate", "Vibrate: On")
        } else {
            Ui.setText("vibrate", "Vibrate: Off")
        }
        Storage.set("sound", sound)
        Storage.set("vibrate", vibrate)
    }
}
