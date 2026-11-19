package settings

import app.Cap
import app.VilaApp
import app.require
import service.Service
import storage.Storage
import system.System
import ui.Action
import ui.Button
import ui.Input
import ui.Screen
import ui.SoftKey
import ui.Ui

class SettingsApp : VilaApp() {
    private var g_theme = "0"
    private var g_large = "0"
    private var g_key_mode = "0"
    private var g_scroll = "0"
    private var g_taps = "0"
    private var g_pkg = "0"
    private var g_filter = ""

    private val searchBox = Input(id = "search", placeholder = "Search settings")
    private val openBtn = Button(id = "btn_open_app", variant = "primary") {
        openSelected()
    }

    private fun themeName(): String {
        if (g_theme == "1") {
            return "Warm Beige"
        }
        return "Sunset Orange"
    }

    private fun keyModeName(): String {
        if (g_key_mode == "1") {
            return "T9"
        }
        if (g_key_mode == "2") {
            return "123"
        }
        return "ABC"
    }

    private fun scrollName(): String {
        if (g_scroll == "1") {
            return "Fast"
        }
        if (g_scroll == "2") {
            return "Precise"
        }
        return "Standard"
    }

    private fun textSizeName(): String {
        if (g_large == "1") {
            return "Large"
        }
        return "Normal"
    }

    private fun savePrefs() {
        Storage.set("theme", g_theme)
        Storage.set("large_text", g_large)
        Storage.set("key_mode", g_key_mode)
        Storage.set("scroll", g_scroll)
        Storage.set("taps", g_taps)
        Storage.set("pkg_sel", g_pkg)
    }

    private fun loadPrefs() {
        val t = Storage.get("theme")
        if (t.isNotBlank()) {
            g_theme = t
        }
        val l = Storage.get("large_text")
        if (l.isNotBlank()) {
            g_large = l
        }
        val k = Storage.get("key_mode")
        if (k.isNotBlank()) {
            g_key_mode = k
        }
        val s = Storage.get("scroll")
        if (s.isNotBlank()) {
            g_scroll = s
        }
        val taps = Storage.get("taps")
        if (taps.isNotBlank()) {
            g_taps = taps
        }
        val p = Storage.get("pkg_sel")
        if (p.isNotBlank()) {
            g_pkg = p
        }
    }

    private fun refreshHub() {
        System.deviceInfo("sum_device")
        System.apiVersion("sum_api")
        Ui.setText("sec_display", "Display " + themeName())
        Ui.setText("sec_input", "Input " + keyModeName())
        Ui.setText("sec_apps", "Apps packages")
        Ui.setText("sec_services", "Services musicd")
        Ui.setText("sec_system", "System memory")
    }

    private fun refreshDisplay() {
        System.display("disp_res")
        System.theme("disp_theme")
        Ui.setText("disp_theme", "Theme: " + themeName())
        Ui.setText("btn_text_size", "Text: " + textSizeName())
        Ui.setText("disp_aspect", "Accent #EA580C on #F5EFE6")
        Ui.setText("preview_row", "Sample " + textSizeName())
        Ui.setText("preview_badge", themeName())
    }

    private fun refreshInput() {
        System.input("input_devices")
        Ui.setText("input_mode", "Key mode: " + keyModeName())
        Ui.setText("input_scroll", "Scroll: " + scrollName())
        Ui.setText("btn_key_mode", "Key mode: " + keyModeName())
        Ui.setText("btn_scroll", "Scroll: " + scrollName())
        Ui.setText("btn_tap_test", "Tap test: " + g_taps)
    }

    private fun refreshApps() {
        System.packages("apps_summary")
        System.storage("apps_summary")
    }

    private fun refreshServices() {
        Service.status("musicd", "svc_music_state")
    }

    private fun refreshSystem() {
        System.deviceInfo("sys_device")
        System.memory("sys_memory")
        System.storage("sys_storage")
        System.apiVersion("sys_api")
        System.locale("sys_locale")
        System.time("sys_time")
    }

    private fun refreshAll() {
        refreshHub()
        refreshDisplay()
        refreshInput()
        refreshApps()
        refreshServices()
        refreshSystem()
        savePrefs()
    }

    private fun selectPkg(index: String) {
        g_pkg = index
        Storage.set("pkg_sel", g_pkg)
        val line = Ui.getText("pkg" + index)
        Ui.setText("pkg_name", line)
        Ui.setText("pkg_id", "slot " + index)
        Ui.setText("pkg_ver", "manifest.xml")
        Ui.setText("pkg_caps", "ui storage system")
        Ui.setText("pkg_entry", "res/layout/main.xml")
        Ui.setText("pkg_badge", "Installed")
        Ui.push("res/layout/app_detail.xml")
    }

    private fun openSelected() {
        if (g_pkg == "0") {
            Action.run("app:dialer")
        } else if (g_pkg == "1") {
            Action.run("app:messages")
        } else if (g_pkg == "2") {
            Action.run("app:contacts")
        } else if (g_pkg == "3") {
            Action.run("app:notes")
        } else if (g_pkg == "4") {
            Action.run("app:calculator")
        } else if (g_pkg == "5") {
            Action.run("app:clock")
        } else if (g_pkg == "6") {
            Action.run("app:files")
        } else if (g_pkg == "7") {
            Action.run("app:browser")
        } else if (g_pkg == "8") {
            Action.run("app:music")
        } else {
            Ui.dialog("Package", "Stay in Settings to keep the stack rooted")
        }
    }

    private fun applySearch() {
        g_filter = Ui.getText("search")
        if (g_filter.isBlank()) {
            System.deviceInfo("sum_device")
        } else {
            Ui.setText("sum_device", "Filter: " + g_filter)
        }
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE, Cap.SYSTEM_POWER, Cap.STORAGE_APP, Cap.STORAGE_FILES, Cap.AUDIO_BG)
        loadPrefs()
        searchBox.attach(screen)
        openBtn.attach(screen)
        Storage.assetRead("about.txt", "preview_font")
        refreshAll()
    }

    override fun onTick() {
        System.memory("sys_memory")
        System.time("sys_time")
        Service.status("musicd", "svc_music_state")
    }

    override fun onAction(action: String) {
        when (action) {
            "search" -> applySearch()
            "theme_sunset" -> {
                g_theme = "0"
                refreshDisplay()
                refreshHub()
                savePrefs()
            }
            "theme_beige" -> {
                g_theme = "1"
                refreshDisplay()
                refreshHub()
                savePrefs()
            }
            "theme_cycle" -> {
                if (g_theme == "0") {
                    g_theme = "1"
                } else {
                    g_theme = "0"
                }
                refreshDisplay()
                refreshHub()
                savePrefs()
            }
            "toggle_text" -> {
                if (g_large == "0") {
                    g_large = "1"
                } else {
                    g_large = "0"
                }
                refreshDisplay()
                savePrefs()
            }
            "tap_test" -> {
                g_taps = (g_taps.toInt() + 1).toString()
                refreshInput()
                savePrefs()
            }
            "cycle_key_mode" -> {
                g_key_mode = ((g_key_mode.toInt() + 1) % 3).toString()
                refreshInput()
                refreshHub()
                savePrefs()
            }
            "cycle_scroll" -> {
                g_scroll = ((g_scroll.toInt() + 1) % 3).toString()
                refreshInput()
                savePrefs()
            }
            "refresh_apps" -> refreshApps()
            "pkg_0" -> selectPkg("0")
            "pkg_1" -> selectPkg("1")
            "pkg_2" -> selectPkg("2")
            "pkg_3" -> selectPkg("3")
            "pkg_4" -> selectPkg("4")
            "pkg_5" -> selectPkg("5")
            "pkg_6" -> selectPkg("6")
            "pkg_7" -> selectPkg("7")
            "pkg_8" -> selectPkg("8")
            "pkg_9" -> selectPkg("9")
            "open_pkg" -> openSelected()
            "svc_start" -> {
                Service.start("musicd")
                refreshServices()
            }
            "svc_stop" -> {
                Service.stop("musicd")
                refreshServices()
            }
            "svc_refresh" -> refreshServices()
            "power_menu" -> System.powerMenu()
            "power_restart" -> System.powerRestart()
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> applySearch()
            SoftKey.CENTER -> refreshAll()
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
