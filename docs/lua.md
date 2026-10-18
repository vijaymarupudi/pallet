# Documentation

## clock

Usage:

```
local clock = require("pallet").clock
```


* `id = clock.setTimeout(time, cb)`
* `id = clock.setInterval(time, cb)`
* `time = clock.currentTime()`
* `clock.clearTimeout(id)`
* `clock.clearInterval(id)`
* `time = clock.timeInMs`



## screen

Usage:

```
local screen = require("pallet").screen
```

* `screen.render()`
* `screen.quit()`
* `screen.clear()`
* `screen.rect(x, y, width, height, color)`
* `screen.strokeRect(x, y, width, height, color)`
* `screen.point(x, y, color)`
* `screen.text(x, y, str, fc, bc, align, baseline)`
* `screen.setOnEvent(cb)`
  * Callback: `cb(event)`
  * Event:
    * `type`: `"MouseButton"`, `"MouseMove"`, `"Key"`, `"Quit"`
    * Key:
      * bool `state`
      * bool `repeat`
      * int `keycode`
      * int `scancode`
      * int `mod`
