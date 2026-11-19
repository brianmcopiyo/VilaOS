package messages

import app.Cap
import app.VilaApp
import app.require
import messages.Messages
import ui.ActionButton
import ui.ListRow
import ui.Screen
import ui.SoftKey
import ui.TextInput
import ui.Ui

class MessagesApp : VilaApp() {
    private val composeInput = TextInput(id = "msg_input", placeholder = "Type message")
    private val sendBtn = ActionButton(id = "send_btn", variant = "primary") {
        Messages.send(composeInput.value(screen))
        composeInput.clear(screen)
    }
    private val threadList = ListRow(id = "thread_list") { index ->
        Messages.openThread(index)
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.MESSAGES)
        composeInput.attach(screen)
        sendBtn.attach(screen)
        threadList.attach(screen)
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Messages.compose()
            SoftKey.CENTER -> Messages.open()
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
