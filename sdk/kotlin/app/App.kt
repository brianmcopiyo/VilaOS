package app

import Screen
import SoftKey

enum class Cap(val id: String) {
    UI("ui"),
    STORAGE("storage.app"),
    STORAGE_APP("storage.app"),
    STORAGE_FILES("storage.files"),
    TELEPHONY("telephony"),
    MESSAGES("messages"),
    CONTACTS("contacts"),
    AUDIO("audio.bg"),
    AUDIO_BG("audio.bg"),
    CLOCK("system.clock"),
    SYSTEM_CLOCK("system.clock"),
    SYSTEM_POWER("system.power"),
    SYSTEM_DEVICE("system.device"),
    NETWORK("network")
}

fun require(vararg caps: Cap) {}

fun vapi(method: String, a1: String = "", a2: String = ""): String = ""

open class VilaApp {
    open fun onStart(screen: Screen) {}
    open fun onLoad() {}
    open fun onClick(id: String) {}
    open fun onSoft(side: SoftKey) {}
    open fun onAction(action: String) {}
    open fun onNumber(value: String) {}
    open fun onTick() {}
}
