package shell

import app.VilaApp
import pkg.PackageManager
import system.System
import Badge
import Button
import Grid
import Input
import Row
import Screen
import Text
import window.WindowManager

class DashboardView : VilaApp() {
    private val sessionBadge = Badge(id = "SessionBadge")
    private val workspaceSubtitle = Text(id = "WorkspaceSubtitle")
    private val ramRow = Row(id = "RamRow")
    private val packagesRow = Row(id = "PackagesRow")
    private val windowsRow = Row(id = "WindowsRow")
    private val activeWindowLabel = Text(id = "ActiveWindowLabel")
    private val quickSearchInput = Input(id = "QuickSearchInput")
    private val appGrid = Grid(id = "DynamicAppGrid")

    private val refreshAppsBtn = Button(id = "RefreshAppsBtn") {
        reloadPackages()
    }

    override fun onStart(screen: Screen) {
        sessionBadge.attach(screen)
        workspaceSubtitle.attach(screen)
        ramRow.attach(screen)
        packagesRow.attach(screen)
        windowsRow.attach(screen)
        activeWindowLabel.attach(screen)
        quickSearchInput.attach(screen)
        appGrid.attach(screen)
        refreshAppsBtn.attach(screen)
        PackageManager.refresh()
        reloadPackages()
    }

    override fun onTick() {
        if (PackageManager.pollChanged() != 0) {
            reloadPackages()
        } else {
            renderAppGrid(quickSearchInput.value(screen))
        }
        refreshTelemetry()
    }

    fun reloadPackages() {
        PackageManager.refresh()
        renderAppGrid(quickSearchInput.value(screen))
        refreshTelemetry()
    }

    private fun renderAppGrid(filter: String) {
        PackageManager.renderGrid("DynamicAppGrid", filter)
    }

    fun refreshTelemetry() {
        val memMb = System.memoryMb()
        val openCount = WindowManager.windowCount()
        val pkgCount = PackageManager.count()
        val focusedTitle = WindowManager.focusedTitle()
        sessionBadge.setText("${memMb} MB")
        ramRow.setSubtitle("${memMb} MB configured")
        packagesRow.setSubtitle("$pkgCount installed")
        windowsRow.setSubtitle("$openCount active")
        workspaceSubtitle.setText("$pkgCount packages · $openCount open windows")
        activeWindowLabel.setText("Focused: " + focusedTitle)
    }
}
