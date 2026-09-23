-- Version checking: where it is offered. The dialog and the settings page
-- are the module's own widgets, in containers the Host owns.

local check = commands.get("com.bktus.gpgfrontend.module.version_checking.check_for_updates")

ui.mount { id = "update", anchor = ui.anchor.dialog {}, widget = native.widget("update") }

ui.mount {
  id = "settings",
  anchor = ui.anchor.settings { section = "application" },
  widget = native.widget("settings"),
}

ui.action { id = "help", anchor = ui.anchor("main.menu.help"), command = check }
