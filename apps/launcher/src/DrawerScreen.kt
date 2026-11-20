package launcher

import system.System
import Ui
class DrawerScreen {
    fun apps() {
        System.packages("title")
        Ui.setText("title", "App Launcher")
        System.time("clock")
    }

    fun filter() {
        val q = Ui.getText("search")
        if (q.isBlank()) {
            apps()
        } else {
            Ui.setText("title", "Apps: " + q)
        }
    }
}
