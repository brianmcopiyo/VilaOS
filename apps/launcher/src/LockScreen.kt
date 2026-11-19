package launcher

import clock.Clock
import system.System
import ui.Ui

class LockScreen {
    fun lock() {
        System.time("clock")
        Clock.date("date")
        Ui.setText("note1", "2 messages")
        Ui.setText("note2", "1 missed call")
    }
}
