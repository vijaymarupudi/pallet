local __pallet = require('__pallet')

return {
   clock = __pallet.clock,
   grid = require("pallet.grid"),
   beatClock = __pallet.beatClock,
   screen = __pallet.screen,
   midi = __pallet.midi,
   quit = __pallet.quit
}
