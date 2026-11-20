package settings

import storage.Storage
import system.System
import Ui
class MainScreen {
    fun refresh() {
        System.deviceInfo("device")
        System.apiVersion("api")
        System.display("display")
        System.input("input")
        System.storage("packages")
        System.memory("system")
        Ui.setText("services", "Services musicd")
    }

    fun search() {
        val q = Ui.getText("search")
        if (q.isBlank()) {
            System.deviceInfo("device")
        } else {
            Ui.setText("device", "Filter: " + q)
            Storage.set("settings_filter", q)
        }
    }
}
