package launcher

import app.Cap
import app.VilaApp
import app.require
import Screen
import SoftKey
class LauncherApp : VilaApp() {
    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE, Cap.SYSTEM_POWER, Cap.STORAGE_APP, Cap.STORAGE_FILES)
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> {}
            SoftKey.CENTER -> {}
            SoftKey.RIGHT -> {}
        }
    }
}
