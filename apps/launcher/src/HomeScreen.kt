package launcher

import clock.Clock
import system.System
import Ui
class HomeScreen {
    fun home() {
        System.time("clock")
        Clock.date("date")
        Ui.setText("statuspill", "Ready")
        Ui.setText("brand", "VilaOS")
        Ui.setText("menu", "Apps")
    }
}
