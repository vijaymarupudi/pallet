local osc = require('pallet').osc
local clock = require('pallet').clock

local server = osc.createServer(9000)

osc.listen(server, function(path, message)
              print(path)
              for i, v in ipairs(message) do
                 print("value", v)
              end
end)

-- clock.setInterval(clock.timeInMs(50), function()
--                      local time = clock.currentTime()
--                      osc.send(port, "/hello", {"wow!", time})
--                      print("hello!", time)
-- end)

