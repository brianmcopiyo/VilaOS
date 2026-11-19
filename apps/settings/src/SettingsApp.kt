package settings

import app.Cap
import app.VilaApp
import app.require
import storage.Storage
import ui.Input
import ui.Screen
import ui.SoftKey
import ui.Ui

class SettingsApp : VilaApp() {
    private var g_theme = "0"
    private var g_large = "0"
    private var g_icons = "0"
    private var g_key_mode = "0"
    private var g_scroll = "0"
    private var g_taps = "0"

    private val searchBox = Input(id = "search", placeholder = "Search settings")

    private fun loadPrefs() {
        val theme = Storage.get("theme")
        if (theme.isNotBlank()) {
            g_theme = theme
        }
        val large = Storage.get("large_text")
        if (large.isNotBlank()) {
            g_large = large
        }
        val icons = Storage.get("icon_set")
        if (icons.isNotBlank()) {
            g_icons = icons
        }
        val mode = Storage.get("key_mode")
        if (mode.isNotBlank()) {
            g_key_mode = mode
        }
        val scroll = Storage.get("scroll")
        if (scroll.isNotBlank()) {
            g_scroll = scroll
        }
        val taps = Storage.get("taps")
        if (taps.isNotBlank()) {
            g_taps = taps
        }
    }

    private fun refreshAll() {
        refresh()
        refreshDisplay(g_theme, g_large, g_icons)
        refreshInput(g_key_mode, g_scroll, g_taps)
        refreshPackages()
        refreshServices()
        refreshSystem()
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE, Cap.SYSTEM_POWER, Cap.STORAGE_APP, Cap.STORAGE_FILES, Cap.AUDIO_BG)
        loadPrefs()
        searchBox.attach(screen)
        Storage.assetRead("about.txt", "disp_adapt")
        refreshAll()
    }

    override fun onTick() {
        refreshSystem()
        refreshServices()
    }

    override fun onAction(action: String) {
        when (action) {
            "search" -> search()
            "disp_theme_sunset" -> {
                g_theme = "0"
                refreshDisplay(g_theme, g_large, g_icons)
                refresh()
            }
            "disp_theme_beige" -> {
                g_theme = "1"
                refreshDisplay(g_theme, g_large, g_icons)
                refresh()
            }
            "disp_theme_cycle" -> {
                if (g_theme == "0") {
                    g_theme = "1"
                } else {
                    g_theme = "0"
                }
                refreshDisplay(g_theme, g_large, g_icons)
                refresh()
            }
            "disp_toggle_text" -> {
                if (g_large == "0") {
                    g_large = "1"
                } else {
                    g_large = "0"
                }
                refreshDisplay(g_theme, g_large, g_icons)
            }
            "disp_cycle_icons" -> {
                g_icons = ((g_icons.toInt() + 1) % 3).toString()
                refreshDisplay(g_theme, g_large, g_icons)
            }
            "inp_tap" -> {
                g_taps = (g_taps.toInt() + 1).toString()
                refreshInput(g_key_mode, g_scroll, g_taps)
            }
            "inp_cycle_mode" -> {
                g_key_mode = ((g_key_mode.toInt() + 1) % 3).toString()
                refreshInput(g_key_mode, g_scroll, g_taps)
                refresh()
            }
            "inp_cycle_scroll" -> {
                g_scroll = ((g_scroll.toInt() + 1) % 3).toString()
                refreshInput(g_key_mode, g_scroll, g_taps)
            }
            "pkg_refresh" -> refreshPackages()
            "pkg_0" -> openDetail("0")
            "pkg_1" -> openDetail("1")
            "pkg_2" -> openDetail("2")
            "pkg_3" -> openDetail("3")
            "pkg_4" -> openDetail("4")
            "pkg_5" -> openDetail("5")
            "pkg_6" -> openDetail("6")
            "pkg_7" -> openDetail("7")
            "pkg_8" -> openDetail("8")
            "pkg_9" -> openDetail("9")
            "pkg_open" -> openSelected()
            "pkg_reload" -> reloadDetail()
            "svc_start" -> startService()
            "svc_stop" -> stopService()
            "svc_status" -> statusService()
            "svc_cmd" -> cmdService()
            "sys_power_menu" -> powerMenu()
            "sys_restart" -> restart()
            "sys_shutdown" -> shutdown()
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> search()
            SoftKey.CENTER -> refreshAll()
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
