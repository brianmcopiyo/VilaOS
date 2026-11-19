package clock

import app.Cap
import app.VilaApp
import app.require
import clock.Clock
import ui.Action
import ui.Screen
import ui.SoftKey
import ui.Ui

class ClockApp : VilaApp() {
    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_CLOCK)
        Clock.now("clock_now")
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Action.run("sw_start")
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
