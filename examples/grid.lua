local grid = require ('pallet').grid
local clock = require('pallet').clock

grid.connect(1, function(g)
  g.onKey:listen(function(x, y, z)
        print(x, y, z)
  end)
  local s = 1
  clock.setInterval(clock.timeInMs(100), function()
    s = 1 - s
    g:all(15 * s)
    g:render()
  end)
end)
