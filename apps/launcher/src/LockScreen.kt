package launcher

import clock.Clock
import system.System
import Ui
class LockScreen {
    fun lock() {
        System.time("clock")
        Clock.date("date")
        Ui.setText("hint", "Desktop session ready")
        Ui.setText("go", "Open desktop")
        Ui.setText("unlock", "Enter desktop")
    }
}
