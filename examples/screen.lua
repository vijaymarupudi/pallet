local beatClock = require('pallet').beatClock
local clock = require('pallet').clock
local screen = require('pallet').screen
local midi = require('pallet').midi
local grid = require('pallet').grid

local state = false

local originx = 1
local originy = 1

local TEXT = ""

local function text(x, y, string, c)
  screen.text(x, y, string, c, 0, screen.Default, screen.Default)
end

grid.connect(1, function(g)
  g:setOnKey(function(x, y, z)
    TEXT = string.format("%d %d %d", x, y, z)
  end)
end)

clock.setInterval(clock.timeInMs(math.floor(1/60 * 1000)), function()
  screen.clear()
  text(30, 30, TEXT, 15)
  screen.render()
end)

screen.setOnEvent(function(event)
  print(event.type)
  if (event.type == "MouseMove") then
    TEXT = string.format("(%f, %f)", event.x, event.y)
    if (event.x ~= 0) then
      beatClock.setBPM(event.x * 2)
    end
    originx = event.x
    originy = event.y
  elseif event.type == "Quit" then
    screen.quit()
  end
end)
