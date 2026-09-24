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

-- The same operations on the keys a key list acts on: the checked ones, or
-- else the selection, sent as one batch. A key group is not a key a server
-- holds, so a target that includes one greys the entries out.
local function for_the_targets(hide_when_empty)
  return function(ctx)
    local keys = ctx.keys
    local none = not keys or #keys == 0
    if none and not ctx:has_key_group() then
      if hide_when_empty then return { visible = false } end
      return { enabled = false }
    end
    if ctx:has_key_group() or none then return { enabled = false } end
    return { args = { keys = keys } }
  end
end

-- Right-clicking a key: shown only when there is something to act on.
ui.action { id = "publish_selected", anchor = ui.anchor("key.list.context"),
            command = publish, update = for_the_targets(true) }
ui.action { id = "refresh_selected", anchor = ui.anchor("key.list.context"),
            command = refresh, update = for_the_targets(true) }

-- Key Management's menu bar: always there, greyed until keys are checked.
ui.action { id = "publish_checked",
            anchor = ui.anchor("key.manager.menu.operations"),
            command = publish, update = for_the_targets(false) }
ui.action { id = "refresh_checked",
            anchor = ui.anchor("key.manager.menu.operations"),
            command = refresh, update = for_the_targets(false) }
