package settings

import app.Cap
import app.VilaApp
import app.require
import system.System
import ui.Screen
import ui.SoftKey
import ui.Ui

class SettingsApp : VilaApp() {
    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE)
        System.deviceInfo("about2")
        System.locale("time_help")
        System.theme("about1")
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> System.powerMenu()
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
