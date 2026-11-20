package launcher

import Ui
class RecentsScreen {
    fun tasks() {
        Ui.setText("title", "Recents")
    }

    fun empty() {
        Ui.setText("r0", "— cleared")
        Ui.setText("r1", "— cleared")
        Ui.setText("r2", "— cleared")
        Ui.setText("r3", "— cleared")
        Ui.setText("r4", "— cleared")
        Ui.setText("r5", "— cleared")
    }
}
