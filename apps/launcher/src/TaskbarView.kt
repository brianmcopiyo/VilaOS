package shell

import app.VilaApp
import Button
import Screen
import window.WindowManager

class TaskbarView : VilaApp() {
    private val homeBtn = Button(id = "HomeBtn") {
        WindowManager.showDesktop()
    }
    private val showDesktopBtn = Button(id = "ShowDesktopBtn") {
        WindowManager.showDesktop()
    }

    override fun onStart(screen: Screen) {
        homeBtn.attach(screen)
        showDesktopBtn.attach(screen)
        WindowManager.syncTaskbar()
    }

    override fun onTick() {
        WindowManager.syncTaskbar()
    }
}
