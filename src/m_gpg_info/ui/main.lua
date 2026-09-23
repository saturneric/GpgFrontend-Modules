-- GnuPG information: where it is offered. The dialog is the module's own
-- widget, in a frame the Host owns; the entry's title is the command's.

local show = commands.get("com.bktus.gpgfrontend.module.gnupg_info_gathering.show_gnupg_info")

ui.mount { id = "info", anchor = ui.anchor.dialog {}, widget = native.widget("info") }

ui.action {
  id = "help",
  anchor = ui.anchor("main.menu.help"),
  command = show,
  icon = ":/icons/key.png",
}
