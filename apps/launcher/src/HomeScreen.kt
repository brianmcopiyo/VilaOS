package launcher

import clock.Clock
import system.System
import ui.Ui

class HomeScreen {
    fun home() {
        System.time("clock")
        Clock.date("date")
        Ui.setText("statuspill", "VilaOS")
        Ui.setText("drawer", "Search apps")
    }
}
