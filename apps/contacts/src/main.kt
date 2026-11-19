package contacts

import app.Cap
import app.VilaApp
import app.require
import contacts.Contacts
import ui.Screen
import ui.SoftKey
import ui.TextInput
import ui.Ui

class ContactsApp : VilaApp() {
    private val filter = TextInput(id = "filter", placeholder = "Search")

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.CONTACTS, Cap.TELEPHONY)
        filter.attach(screen)
    }

    override fun onAction(action: String) {
        when (action) {
            "call" -> Contacts.call()
            "search" -> {
                val q = Ui.getText("filter")
                Contacts.search(q)
            }
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Contacts.call()
            SoftKey.CENTER -> Contacts.open()
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
