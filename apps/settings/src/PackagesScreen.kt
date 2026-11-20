package settings

import storage.Storage
import system.System
import Action
import Ui
class PackagesScreen {
    fun refreshPackages() {
        System.packages("pkg_summary")
        System.storage("pkg_summary")
    }

    fun openDetail(index: String) {
        Storage.set("pkg_sel", index)
        System.packageDetail(index, "pkg_id")
        Ui.push("res/layout/app_detail.xml")
    }

    fun reloadDetail() {
        val index = Storage.get("pkg_sel")
        if (index.isBlank()) {
            System.packageDetail("0", "pkg_id")
        } else {
            System.packageDetail(index, "pkg_id")
        }
    }

    fun openSelected() {
        val index = Storage.get("pkg_sel")
        System.packageDetail(index, "pkg_id")
        val id = Ui.getText("pkg_id")
        if (id.isBlank()) {
            Ui.dialog("Package", "No package selected")
        } else {
            Action.run("app:" + id)
        }
    }
}
