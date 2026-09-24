-- Instant messaging: where it is offered.
--
-- Decrypting a token needs no entry at all: the module's decoder is offered
-- every text the user decrypts. These are the two ways to make one, and the
-- page to set the shared phrase.

local encrypt = commands.get("com.bktus.gpgfrontend.module.im.encrypt")
local encrypt_sign = commands.get("com.bktus.gpgfrontend.module.im.encrypt_sign")

ui.mount {
  id = "settings",
  anchor = ui.anchor.settings { section = "features" },
  widget = native.widget("settings"),
}

-- A token only means something when the editor's text IS the document.
-- Whether an encryption applies right now is the commands' own state, which
-- asks the Host's operations each time the menu is shown.
local function for_plain_text(ctx)
  if not ctx.document or ctx.document.type ~= "text" then
    return { enabled = false }
  end
  return { args = { target = ctx.document } }
end

ui.action { id = "encrypt", anchor = ui.anchor("main.menu.operations"),
            command = encrypt, icon = ":/icons/email.png",
            shortcut = "Ctrl+M", update = for_plain_text }
ui.action { id = "encrypt_sign", anchor = ui.anchor("main.menu.operations"),
            command = encrypt_sign, icon = ":/icons/email-check.png",
            shortcut = "Ctrl+Shift+M", update = for_plain_text }
