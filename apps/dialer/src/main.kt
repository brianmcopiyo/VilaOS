package dialer

import app.Cap
import app.VilaApp
import app.require
import contacts.Contacts
import telephony.Telephony
import ui.Keypad
import ui.Screen
import ui.SoftKey
import ui.Ui

class DialerApp : VilaApp() {
    private val keypad = Keypad(id = "dialpad") { key ->
        Telephony.digit(key)
        refreshMatch()
    }

    private fun refreshMatch() {
        val num = Ui.getText("number")
        if (num.isBlank()) {
            Ui.setText("contact_match", "Enter a number")
        } else {
            Contacts.search(num)
        }
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.TELEPHONY, Cap.CONTACTS)
        keypad.attach(screen)
        refreshMatch()
    }

    override fun onAction(action: String) {
        when (action) {
            "call" -> Telephony.call()
            "clear" -> {
                Telephony.clear()
                refreshMatch()
            }
            "end_call" -> Telephony.end()
            "mute" -> Telephony.mute()
            "speaker" -> Telephony.speaker()
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Telephony.call()
            SoftKey.CENTER -> Telephony.call()
            SoftKey.RIGHT -> {
                Telephony.clear()
                refreshMatch()
            }
        }
    }
}
