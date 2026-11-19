package dialer

import app.Cap
import app.VilaApp
import app.require
import telephony.Telephony
import ui.DialKeypad
import ui.Screen
import ui.SoftKey

class DialerApp : VilaApp() {
    private val keypad = DialKeypad(id = "dialpad") { key ->
        Telephony.digit(key)
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.TELEPHONY)
        keypad.attach(screen)
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Telephony.call()
            SoftKey.CENTER -> Telephony.call()
            SoftKey.RIGHT -> Telephony.clear()
        }
    }
}
