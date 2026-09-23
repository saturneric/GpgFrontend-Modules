-- OpenPGP structure inspector: where it is offered.
--
-- What the entry does, what it is called and when it is enabled all belong
-- to the module's command; this file only places it. The dialog is the
-- module's own widget, in a frame the Host owns.

local open = commands.get("com.bktus.gpgfrontend.module.pgp_inspect.open_inspector")

ui.mount {
  id = "inspector",
  anchor = ui.anchor.dialog {},
  widget = native.widget("inspector"),
}

ui.action { id = "menu", anchor = ui.anchor("main.menu.advanced"), command = open }
ui.action { id = "context", anchor = ui.anchor("editor.context"), command = open }
