package calculator

import app.Cap
import app.VilaApp
import app.require
import ui.Screen
import ui.SoftKey
import ui.Ui

class CalculatorApp : VilaApp() {
    private var g_acc = "0"
    private var g_cur = ""
    private var g_op = ""
    private var g_fresh = "1"
    private var g_dot = "0"

    private fun show() {
        if (g_cur.isNotBlank()) {
            Ui.setText("calc_display", g_cur)
        } else {
            Ui.setText("calc_display", g_acc)
        }
        if (g_op.isNotBlank()) {
            Ui.setText("calc_expr", g_acc + " " + g_op)
        } else {
            Ui.setText("calc_expr", "")
        }
    }

    private fun pushDigit(d: String) {
        if (g_fresh == "1") {
            g_cur = d
            g_fresh = "0"
            g_dot = "0"
        } else {
            g_cur = g_cur + d
        }
        show()
    }

    private fun applyOp(next: String) {
        if (g_cur.isNotBlank()) {
            if (g_op == "+") {
                g_acc = (g_acc.toInt() + g_cur.toInt()).toString()
            } else if (g_op == "-") {
                g_acc = (g_acc.toInt() - g_cur.toInt()).toString()
            } else if (g_op == "*") {
                g_acc = (g_acc.toInt() * g_cur.toInt()).toString()
            } else if (g_op == "/") {
                if (g_cur.toInt() != 0) {
                    g_acc = (g_acc.toInt() / g_cur.toInt()).toString()
                }
            } else {
                g_acc = g_cur
            }
            g_cur = ""
        }
        if (next == "=") {
            g_op = ""
            g_fresh = "1"
            g_dot = "0"
        } else {
            g_op = next
            g_fresh = "1"
            g_dot = "0"
        }
        show()
    }

    private fun clearAll() {
        g_acc = "0"
        g_cur = ""
        g_op = ""
        g_fresh = "1"
        g_dot = "0"
        show()
    }

    override fun onStart(screen: Screen) {
        require(Cap.UI, Cap.SYSTEM_DEVICE)
        show()
    }

    override fun onAction(action: String) {
        when (action) {
            "calc:clear" -> clearAll()
            "calc:eq" -> applyOp("=")
            "calc:+" -> applyOp("+")
            "calc:-" -> applyOp("-")
            "calc:*" -> applyOp("*")
            "calc:/" -> applyOp("/")
            "calc:d:0" -> pushDigit("0")
            "calc:d:1" -> pushDigit("1")
            "calc:d:2" -> pushDigit("2")
            "calc:d:3" -> pushDigit("3")
            "calc:d:4" -> pushDigit("4")
            "calc:d:5" -> pushDigit("5")
            "calc:d:6" -> pushDigit("6")
            "calc:d:7" -> pushDigit("7")
            "calc:d:8" -> pushDigit("8")
            "calc:d:9" -> pushDigit("9")
            "calc:dot" -> {
                if (g_fresh == "1") {
                    g_cur = "0."
                    g_fresh = "0"
                    g_dot = "1"
                } else if (g_dot == "0") {
                    g_cur = g_cur + "."
                    g_dot = "1"
                }
                show()
            }
        }
    }

    override fun onSoft(side: SoftKey) {
        when (side) {
            SoftKey.LEFT -> clearAll()
            SoftKey.CENTER -> applyOp("=")
            SoftKey.RIGHT -> Ui.pop()
        }
    }
}
