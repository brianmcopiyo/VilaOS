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
        val body = composeInput.value(screen)
        if (body.isNotBlank()) {
            Messages.send(body, "msgstatus")
            composeInput.clear(screen)
        }
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

    override fun onAction(action: String) {
        when (action) {
            "send" -> {
                val body = Ui.getText("msg_input")
                if (body.isBlank()) {
                    val alt = Ui.getText("msg")
                    if (alt.isNotBlank()) {
                        Messages.send(alt, "msgstatus")
                    }
                } else {
                    Messages.send(body, "msgstatus")
                    Ui.clearText("msg_input")
                }
            }
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> Messages.compose()
            SoftKey.CENTER -> Messages.open()
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
