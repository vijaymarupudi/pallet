local grid = require ('pallet').grid
local clock = require('pallet').clock
local screen = require('pallet').screen
local pallet = require('pallet')

clock.setInterval(clock.timeInMs(1000 / 60), function()
                                    screen.rect(1, 1, 10, 10, 15)
                                    screen.render()
end)

screen.setOnEvent(function(e)
      if e.type == 'Quit' then
            pallet.quit()
      end
end)



grid.connect(1, function(g)
                g:listen(function(x, y, z)
                      print(x, y, z)
                end)
                local s = 1
                clock.setInterval(clock.timeInMs(100), function()
                                     s = 1 - s
                                     g:all(15 * s)
                                     g:render()
                end)
end)
