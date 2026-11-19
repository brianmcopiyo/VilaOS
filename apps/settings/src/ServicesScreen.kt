package settings

import service.Service
import ui.Ui

class ServicesScreen {
    fun refreshServices() {
        Service.status("musicd", "svc_music_state")
        Service.cmd("musicd", "STATUS", "svc_music_track")
        val st = Ui.getText("svc_music_state")
        if (st.isBlank()) {
            Ui.setText("svc_music_badge", "idle")
        } else {
            Ui.setText("svc_music_badge", st)
        }
    }

    fun startService() {
        Service.start("musicd")
        refreshServices()
    }

    fun stopService() {
        Service.stop("musicd")
        refreshServices()
    }

    fun statusService() {
        refreshServices()
    }

    fun cmdService() {
        Service.cmd("musicd", "STATUS", "svc_music_track")
        refreshServices()
    }
}
