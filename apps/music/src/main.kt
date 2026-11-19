package music

import app.Cap
import app.VilaApp
import app.require
import audio.Audio
import service.Service
import ui.Button
import ui.Screen
import ui.SoftKey
import ui.Ui

class MusicApp : VilaApp() {
    private val playBtn = Button(id = "btn_play", variant = "primary") {
        Audio.play()
        Service.status("musicd", "svc_state")
    }
    private val pauseBtn = Button(id = "btn_pause", variant = "secondary") {
        Audio.pause()
        Service.status("musicd", "svc_state")
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.AUDIO_BG)
        Service.start("musicd")
        Service.status("musicd", "svc_state")
        Service.cmd("musicd", "STATUS", "now_playing")
        playBtn.attach(screen)
        pauseBtn.attach(screen)
    }

    override fun onTick() {
        Service.status("musicd", "svc_state")
        Service.cmd("musicd", "STATUS", "now_playing")
    }

    override fun onAction(action: String) {
        when (action) {
            "music_play" -> {
                Audio.play()
                Service.status("musicd", "svc_state")
            }
            "music_pause" -> {
                Audio.pause()
                Service.status("musicd", "svc_state")
            }
            "music_skip" -> {
                Audio.skip()
                Service.status("musicd", "svc_state")
            }
            "music_stop_svc" -> Audio.stop("now_playing", "svc_state")
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> {
                Audio.play()
                Service.status("musicd", "svc_state")
            }
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
