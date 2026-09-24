-- Copyright (C) 2026 Tap Zap.
-- SPDX-License-Identifier: GPL-3.0-or-later
-- Omarchy/Hyprland Lua integration for the separate TAP ZAP trial only.
-- A desktop or layer-shell click need not change an XWayland window's focus.
-- Observe ordinary clicks without consuming them or changing other bindings.
local previous = rawget(_G, "tapzap_omarchy_outside_click")
if previous then
  for _, binding in ipairs(previous) do binding:remove() end
end
local bindings = {}
_G.tapzap_omarchy_outside_click = bindings

local function dismissOutside()
  local cursor = hl.get_cursor_pos()
  if not cursor then return end
  for _, window in ipairs(hl.get_windows({ class = "TapZapOmarchy", mapped = true })) do
    if window.class == "TapZapOmarchy" and window.visible then
      local pos, size = window.at, window.size
      if cursor.x < pos.x or cursor.y < pos.y or
         cursor.x >= pos.x + size.x or cursor.y >= pos.y + size.y then
        -- Only spawn a command when this trial is visible and clicked outside.
        hl.exec_cmd('"$HOME/.local/bin/tapzap-omarchy" hide')
        return
      end
    end
  end
end

for _, button in ipairs({ 272, 273, 274 }) do
  bindings[#bindings + 1] = hl.bind("mouse:" .. button, dismissOutside, {
    non_consuming = true,
    transparent = true,
    description = "TAP ZAP trial: dismiss on outside click"
  })
end
