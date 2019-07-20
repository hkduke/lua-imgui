local window = require 'imgui.window'
local render = require 'imgui.render'
local imgui = require 'imgui'
local widget = imgui.widget

local _vk = {
    Tab = window.vk.TAB,
    LeftArrow = window.vk.LEFT,
    RightArrow = window.vk.RIGHT,
    UpArrow = window.vk.UP,
    DownArrow = window.vk.DOWN,
    PageUp = window.PRIOR,
    PageDown = window.NEXT,
    Home = window.HOME,
    End = window.END,
    Insert = window.INSERT,
    Delete = window.DELETE,
    Backspace = window.BACK,
    Space = window.SPACE,
    Enter = window.RETURN,
    Escape = window.ESCAPE,
}

local function onidle()
  imgui.SetDisplaySize(window.getsize())
  local x,y = window.getcursorpos()
  x,y = window.screen2client(x,y)
  imgui.NewFrame()
  imgui.SetMousePos(x,y)
  widget.Text('水寒')
  widget.InputText('水寒',{})
  render.render(window.getdc())
end

local function oncreate()
  imgui.Create()
  imgui.LoadFont('simsun.ttc',18.0)
  render.init(window.getdc(),window.getsize())
  imgui.KeyMap(_vk)
end

local function onmousedown(type)
  local button = {
    [window.wm.LBUTTONDOWN] = 0,
    [window.wm.RBUTTONDOWN] = 1,
    [window.wm.MBUTTONDOWN] = 2,
    [window.wm.LBUTTONDBLCLK] = 0,
    [window.wm.RBUTTONDBLCLK] = 1,
    [window.wm.MBUTTONDBLCLK] = 2,
  }
  imgui.MouseState(button[type],1)
  return 0
end

local function onmouseup(type)
  local button = {
    [window.wm.LBUTTONUP] = 0,
    [window.wm.RBUTTONUP] = 1,
    [window.wm.MBUTTONUP] = 2,
  }
  imgui.MouseState(button[type],0)
  return 0
end

local function _getkeystate()
  local ctrl = (window.getkeystate(window.vk.CONTROL) & 0x8000) ~= 0
  ctrl = ctrl and 1 or 0
  local shift = (window.getkeystate(window.vk.SHIFT) & 0x8000) ~= 0
  shift = shift and 1 or 0
  local alt = (window.getkeystate(window.vk.MENU) & 0x8000) ~= 0
  alt = alt and 1 or 0
  local supper = 0
  return ctrl | (shift << 1) | (alt << 2) | (supper << 3)
end

local function onkeyup(wp)
  imgui.KeyState(wp,0,_getkeystate())
  return 0
end

local function onkeydown(wp)
  imgui.KeyState(wp,1,_getkeystate())
  return 0
end

local function onmousewheel(wp)
  return 0
end

local function onchar(wp)
  if wp ~= 0 then
    imgui.InputChar(wp)
  end
  return 0
end

local function onmessage(hwnd,type,wp,lp)
  local mouseup = {
     [window.wm.LBUTTONUP] = true,
     [window.wm.RBUTTONUP] = true,
     [window.wm.MBUTTONUP] = true,
  } 
  local mousedown = {
    [window.wm.LBUTTONDOWN] = true,
    [window.wm.RBUTTONDOWN] = true,
    [window.wm.MBUTTONDOWN] = true,
    [window.wm.MBUTTONDBLCLK] = true,
    [window.wm.RBUTTONDBLCLK] = true,
    [window.wm.LBUTTONDBLCLK] = true,
  }
  local char = {
    [window.wm.CHAR] = true,
    [window.wm.IME_CHAR] = true,
  }
  local keyup = {
    [window.wm.KEYUP] = true,
    [window.wm.SYSKEYUP] = true,
  }
  local keydown = {
    [window.wm.KEYDOWN] = true,
    [window.wm.SYSKEYDOWN] = true,
  }
  if mousedown[type] then
    return onmousedown(type)
  elseif mouseup[type] then
    return onmouseup(type)
  elseif keyup[type] then
    return onkeyup(wp)
  elseif keydown[type] then
    return onkeydown(wp)
  elseif char[type] then
    return onchar(wp)
  elseif type == window.wm.MOUSEWHEEL then
    return onmousewheel(wp)
  else
    return nil
  end
end

window.create('abc',1280,960)
window.on('idle',onidle)
window.on('create',oncreate)
window.on('message',onmessage)
window.show()
window.loop()