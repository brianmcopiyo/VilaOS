package settings

import app.Cap
import app.VilaApp
import app.require
import storage.Storage
import system.System
import ui.Screen
import ui.SoftKey
import ui.Ui

class SettingsApp : VilaApp() {
    private var g_sound = "1"
    private var g_vibrate = "1"
    private var g_large = "0"
    private var g_profile = "0"

    private fun profileName(): String {
        if (g_profile == "1") {
            return "Silent"
        }
        if (g_profile == "2") {
            return "Meeting"
        }
        if (g_profile == "3") {
            return "Outdoor"
        }
        return "General"
    }

    private fun refresh() {
        if (g_sound == "1") {
            Ui.setText("toggle_sound", "Sound: On")
        } else {
            Ui.setText("toggle_sound", "Sound: Off")
        }
        if (g_vibrate == "1") {
            Ui.setText("toggle_vibrate", "Vibrate: On")
        } else {
            Ui.setText("toggle_vibrate", "Vibrate: Off")
        }
        if (g_large == "1") {
            Ui.setText("toggle_text", "Text: Large")
        } else {
            Ui.setText("toggle_text", "Text: Normal")
        }
        Ui.setText("toggle_profile", "Profile: " + profileName())
        Storage.set("sound", g_sound)
        Storage.set("vibrate", g_vibrate)
        Storage.set("large_text", g_large)
        Storage.set("profile", g_profile)
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE, Cap.STORAGE_APP)
        System.deviceInfo("about2")
        System.locale("time_help")
        System.theme("about1")
        val s = Storage.get("sound")
        if (s.isNotBlank()) {
            g_sound = s
        }
        val v = Storage.get("vibrate")
        if (v.isNotBlank()) {
            g_vibrate = v
        }
        val t = Storage.get("large_text")
        if (t.isNotBlank()) {
            g_large = t
        }
        val p = Storage.get("profile")
        if (p.isNotBlank()) {
            g_profile = p
        }
        refresh()
    }

    override fun onAction(action: String) {
        when (action) {
            "toggle_sound" -> {
                if (g_sound == "1") {
                    g_sound = "0"
                } else {
                    g_sound = "1"
                }
                refresh()
            }
            "toggle_vibrate" -> {
                if (g_vibrate == "1") {
                    g_vibrate = "0"
                } else {
                    g_vibrate = "1"
                }
                refresh()
            }
            "toggle_text" -> {
                if (g_large == "1") {
                    g_large = "0"
                } else {
                    g_large = "1"
                }
                refresh()
            }
            "cycle_profile" -> {
                g_profile = ((g_profile.toInt() + 1) % 4).toString()
                refresh()
            }
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> System.powerMenu()
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
