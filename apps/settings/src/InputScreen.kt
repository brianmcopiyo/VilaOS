package settings

import storage.Storage
import system.System
import Ui
class InputScreen {
    fun keyLabel(mode: String): String {
        if (mode == "1") {
            return "T9"
        }
        if (mode == "2") {
            return "123"
        }
        return "ABC"
    }

    fun scrollLabel(scroll: String): String {
        if (scroll == "1") {
            return "Fast"
        }
        if (scroll == "2") {
            return "Precise"
        }
        return "Standard"
    }

    fun refreshInput(mode: String, scroll: String, taps: String) {
        System.input("inp_devices")
        Ui.setText("inp_mode", "Key mode: " + keyLabel(mode))
        Ui.setText("inp_nav", "Scroll: " + scrollLabel(scroll))
        Ui.setText("inp_feedback", "taps=" + taps + " mode=" + keyLabel(mode))
        Ui.setText("inp_btn_tap", "Tap / click test (" + taps + ")")
        Ui.setText("inp_btn_mode", "Cycle key mode (" + keyLabel(mode) + ")")
        Ui.setText("inp_btn_scroll", "Cycle scroll (" + scrollLabel(scroll) + ")")
        Storage.set("key_mode", mode)
        Storage.set("scroll", scroll)
        Storage.set("taps", taps)
    }
}
