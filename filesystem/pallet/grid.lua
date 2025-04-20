local __grid = require('__pallet').grid

local grid = {}

local function wrapGrid(g)
   return {
      g = g,
      led = function(self, x, y, z)
         return __grid.led(self.g, x, y, z)
      end,
      all = function(self, n)
         return __grid.all(self.g, n)
      end,
      clear = function(self)
         return __grid.clear(self.g)
      end,
      render = function(self)
         return __grid.render(self.g)
      end,
      onKey = {
         g = g,
         listen = function(self, cb)
            return __grid.onKeyListen(self.g, cb)
         end,
         unlisten = function(self, id)
            return __grid.onKeyUnlisten(self.g, id)
         end
      },
      setOnKey = function(self, cb)
         return __grid.setOnKey(self.g, cb)
      end
   }
end

function grid.connect(id, cb)
   __grid.connect(id, function(g)
                     cb(wrapGrid(g))
   end)
end

return grid
