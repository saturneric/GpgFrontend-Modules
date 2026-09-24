-- Key server sync: where it is offered.
--
-- The commands do the work and carry the titles; the search dialog and the
-- settings page are the module's own widgets, in containers the Host owns.

local search = commands.get("com.bktus.gpgfrontend.module.key_server_sync.search_key")
local publish = commands.get("com.bktus.gpgfrontend.module.key_server_sync.publish_key")
local refresh = commands.get("com.bktus.gpgfrontend.module.key_server_sync.refresh_key")
local check = commands.get("com.bktus.gpgfrontend.module.key_server_sync.check_publication")

ui.mount { id = "search", anchor = ui.anchor.dialog {}, widget = native.widget("search") }

ui.mount {
  id = "settings",
  anchor = ui.anchor.settings { section = "keys_engines" },
  widget = native.widget("settings"),
}

ui.action {
  id = "search",
  anchor = ui.anchor("main.menu.import_key"),
  command = search,
  icon = ":/icons/import_key_from_server.png",
}

-- Both act on the key the details dialog is showing. They share a category,
-- so the Host groups them under one "Key Server Operations" button.
local function for_this_key(ctx)
  if not ctx.key then return { visible = false } end
  return { args = { key = ctx.key } }
end

ui.action { id = "publish", anchor = ui.anchor("key.details.actions"),
            command = publish, update = for_this_key }
ui.action { id = "refresh", anchor = ui.anchor("key.details.actions"),
            command = refresh, update = for_this_key }
ui.action { id = "check", anchor = ui.anchor("key.details.actions"),
            command = check, update = for_this_key }

-- The same operations on whatever a key list has selected: one key or many,
-- sent as one batch.
local function for_the_selection(ctx)
  local keys = ctx.keys
  if not keys or #keys == 0 then return { visible = false } end
  return { args = { keys = keys } }
end

ui.action { id = "publish_selected", anchor = ui.anchor("key.list.context"),
            command = publish, update = for_the_selection }
ui.action { id = "refresh_selected", anchor = ui.anchor("key.list.context"),
            command = refresh, update = for_the_selection }
