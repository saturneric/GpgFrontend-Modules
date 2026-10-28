-- SPDX-License-Identifier: GPL-3.0-or-later
-- Encrypted notes: a tab per note, whose file only this module reads and
-- writes (persistence = "module"), and one entry in File > Workspace.
-- Unsaved notes are kept in the Host's recovery cache, like any other tab.

local new_note = commands.get("com.bktus.gpgfrontend.module.notes.new_note")

ui.mount {
  id = "editor",
  anchor = ui.anchor.editor {
    document_type = "note",
    extensions = { "textpack.gpg", "textpack" },
    persistence = "module",
    compact_status = true,
  },
  widget = native.factory("editor"),
}

ui.action {
  id = "new",
  anchor = ui.anchor("main.menu.file.workspace"),
  command = new_note,
  icon = ":/icons/lock.png",
}
