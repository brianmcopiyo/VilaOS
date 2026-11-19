package calculator

import app.Cap
import app.VilaApp
import app.require
import ui.Action
import ui.Screen
import ui.SoftKey
import ui.Ui

class CalculatorApp : VilaApp() {
    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE)
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Action.run("calc:clear")
            SoftKey.CENTER -> Action.run("calc:eq")
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
