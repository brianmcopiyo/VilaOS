package clock

import app.Cap
import app.VilaApp
import app.require
import clock.Clock
import ui.Screen
import ui.SoftKey
import ui.Ui

class ClockApp : VilaApp() {
    private var g_sw_run = "0"
    private var g_sw_sec = "0"
    private var g_tm_run = "0"
    private var g_tm_sec = "60"
    private var g_tick = "0"

    private fun fmt(sec: String): String {
        val s = sec.toInt()
        val m = s / 60
        val r = s % 60
        var ms = m.toString()
        var rs = r.toString()
        if (m < 10) {
            ms = "0" + ms
        }
        if (r < 10) {
            rs = "0" + rs
        }
        return ms + ":" + rs
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_CLOCK)
        Clock.now("clock_now")
        Ui.setText("stopwatch_time", fmt(g_sw_sec))
        Ui.setText("timer_time", fmt(g_tm_sec))
    }

    override fun onTick() {
        Clock.now("clock_now")
        g_tick = (g_tick.toInt() + 1).toString()
        if (g_tick.toInt() >= 5) {
            g_tick = "0"
            if (g_sw_run == "1") {
                g_sw_sec = (g_sw_sec.toInt() + 1).toString()
                Ui.setText("stopwatch_time", fmt(g_sw_sec))
            }
            if (g_tm_run == "1") {
                if (g_tm_sec.toInt() > 0) {
                    g_tm_sec = (g_tm_sec.toInt() - 1).toString()
                    Ui.setText("timer_time", fmt(g_tm_sec))
                } else {
                    g_tm_run = "0"
                }
            }
        }
    }

    override fun onAction(action: String) {
        when (action) {
            "sw_start" -> g_sw_run = "1"
            "sw_stop" -> g_sw_run = "0"
            "sw_reset" -> {
                g_sw_run = "0"
                g_sw_sec = "0"
                Ui.setText("stopwatch_time", fmt(g_sw_sec))
            }
            "tm_start" -> g_tm_run = "1"
            "tm_stop" -> g_tm_run = "0"
            "tm_reset" -> {
                g_tm_run = "0"
                g_tm_sec = "60"
                Ui.setText("timer_time", fmt(g_tm_sec))
            }
            "tm_plus" -> {
                g_tm_sec = (g_tm_sec.toInt() + 30).toString()
                Ui.setText("timer_time", fmt(g_tm_sec))
            }
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> g_sw_run = "1"
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
