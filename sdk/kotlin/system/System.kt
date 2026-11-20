package system

object System {
    fun deviceInfo(widgetId: String) {}
    fun display(widgetId: String) {}
    fun graphics(widgetId: String) {}
    fun memory(widgetId: String) {}
    fun memoryMb(): Int = 0
    fun storage(widgetId: String) {}
    fun input(widgetId: String) {}
    fun packages(widgetId: String) {}
    fun packageDetail(index: String, widgetId: String = "") {}
    fun runtime(widgetId: String) {}
    fun time(widgetId: String) {}
    fun locale(widgetId: String) {}
    fun theme(widgetId: String) {}
    fun powerMenu() {}
    fun powerOff() {}
    fun powerRestart() {}
    fun apiVersion(widgetId: String) {}
}
