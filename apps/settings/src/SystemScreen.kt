package settings

import system.System

class SystemScreen {
    fun refreshSystem() {
        System.deviceInfo("sys_device")
        System.memory("sys_memory")
        System.storage("sys_storage")
        System.runtime("sys_runtime")
        System.apiVersion("sys_api")
        System.locale("sys_locale")
        System.time("sys_time")
    }

    fun powerMenu() {
        System.powerMenu()
    }

    fun restart() {
        System.powerRestart()
    }

    fun shutdown() {
        System.powerOff()
    }
}
