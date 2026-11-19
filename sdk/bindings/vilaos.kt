import VilaApp
import UiApi
import Cap
import Storage
import Clock
import System

fun demo() {
    require(Cap.UI, Cap.STORAGE_APP, Cap.STORAGE_FILES, Cap.TELEPHONY, Cap.MESSAGES, Cap.CONTACTS, Cap.AUDIO_BG, Cap.SYSTEM_CLOCK, Cap.SYSTEM_POWER, Cap.SYSTEM_DEVICE)
    UiApi.setText("status", "Ready")
    Storage.set("demo_key", "hello")
    Storage.get("demo_key", "status")
    Clock.now("status")
    System.deviceInfo("status")
}
