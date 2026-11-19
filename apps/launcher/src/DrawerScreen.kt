package launcher

import system.System
import ui.Ui

class DrawerScreen {
    fun apps() {
        System.packages("title")
        Ui.setText("title", "Apps")
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
