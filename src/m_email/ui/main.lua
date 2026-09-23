-- E-mail: where it is offered.
--
-- The message view is the module's own widget, mounted for every document of
-- type "email" and for .eml files, which the Host opens and saves itself.
-- The settings page and the IMAP controller are the module's too. Titles
-- belong to the commands.

local new_message = commands.get("com.bktus.gpgfrontend.module.email.new_message")
local open_imap = commands.get("com.bktus.gpgfrontend.module.email.open_imap_controller")

ui.mount {
  id = "editor",
  anchor = ui.anchor.editor { document_type = "email", extensions = { "eml" } },
  widget = native.factory("editor"),
}

ui.mount {
  id = "settings",
  anchor = ui.anchor.settings { section = "features" },
  widget = native.widget("settings"),
}

ui.mount { id = "imap", anchor = ui.anchor.dialog {}, widget = native.widget("imap") }

ui.action {
  id = "new",
  anchor = ui.anchor("main.menu.file.workspace"),
  command = new_message,
  icon = ":/icons/email.png",
}

ui.action {
  id = "imap",
  anchor = ui.anchor("main.menu.advanced"),
  command = open_imap,
  icon = ":/icons/receive_email.png",
}
