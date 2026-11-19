package launcher

import app.Cap
import app.VilaApp
import app.require
import clock.Clock
import storage.Storage
import system.System
import ui.Input
import ui.Screen
import ui.SoftKey
import ui.Ui

class LauncherApp : VilaApp() {
    private var g_sound = "1"
    private var g_vibrate = "1"

    private val searchBox = Input(id = "search", placeholder = "Search apps")

    private fun loadPrefs() {
        val s = Storage.get("sound")
        if (s.isNotBlank()) {
            g_sound = s
        }
        val v = Storage.get("vibrate")
        if (v.isNotBlank()) {
            g_vibrate = v
        }
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE, Cap.SYSTEM_POWER, Cap.STORAGE_APP, Cap.STORAGE_FILES)
        if (g_sound.isBlank()) {
            g_sound = "1"
        }
        if (g_vibrate.isBlank()) {
            g_vibrate = "1"
        }
        loadPrefs()
        searchBox.attach(screen)
    }

    override fun onTick() {
        System.time("clock")
        System.memory("memory")
        Clock.date("date")
    }

    override fun onAction(action: String) {
        when (action) {
            "search" -> filter()
            "clear" -> empty()
            "unlock" -> Ui.replace("res/layout/home.xml")
            "shade" -> {
                panel(g_sound, g_vibrate)
                Ui.push("res/layout/shade.xml")
            }
            "drawer" -> {
                apps()
                Ui.push("res/layout/drawer.xml")
            }
            "toggle_sound" -> {
                if (g_sound == "1") {
                    g_sound = "0"
                } else {
                    g_sound = "1"
                }
                panel(g_sound, g_vibrate)
            }
            "toggle_vibrate" -> {
                if (g_vibrate == "1") {
                    g_vibrate = "0"
                } else {
                    g_vibrate = "1"
                }
                panel(g_sound, g_vibrate)
            }
            "power_menu" -> System.powerMenu()
            "recent_0" -> Ui.pop()
            "recent_1" -> Ui.pop()
            "recent_2" -> Ui.pop()
            "recent_3" -> Ui.pop()
            "recent_4" -> Ui.pop()
            "recent_5" -> Ui.pop()
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> {
                filter()
                empty()
            }
            SoftKey.CENTER -> {}
            SoftKey.RIGHT -> {}
        }
    }
}
