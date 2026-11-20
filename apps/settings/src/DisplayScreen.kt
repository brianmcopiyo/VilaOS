package settings

import storage.Storage
import system.System
import Ui
class DisplayScreen {
    fun themeLabel(theme: String): String {
        if (theme == "1") {
            return "Warm Beige"
        }
        return "Sunset Orange"
    }

    fun textLabel(large: String): String {
        if (large == "1") {
            return "Large"
        }
        return "Normal"
    }

    fun iconLabel(set: String): String {
        if (set == "1") {
            return "Tabler"
        }
        if (set == "2") {
            return "Phosphor"
        }
        return "Lucide"
    }

    fun refreshDisplay(theme: String, large: String, icons: String) {
        System.display("disp_res")
        System.graphics("disp_gfx")
        System.theme("disp_theme")
        Ui.setText("disp_theme", "Theme: " + themeLabel(theme))
        Ui.setText("disp_font", "Text: " + textLabel(large))
        Ui.setText("disp_adapt", "Containers adapt; glyphs stay 1:1 px")
        Ui.setText("disp_btn_text", "Text size: " + textLabel(large))
        Ui.setText("disp_btn_icon", "Icon set: " + iconLabel(icons))
        Ui.setText("disp_preview_row", "Sample " + textLabel(large))
        Ui.setText("disp_preview_badge", themeLabel(theme))
        Storage.set("theme", theme)
        Storage.set("large_text", large)
        Storage.set("icon_set", icons)
    }
}
