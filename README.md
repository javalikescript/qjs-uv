## Overview

[libuv](https://libuv.org/) bindings for [QuickJS](https://bellard.org/quickjs/)

The QuickJS libuv module, qjs-uv, exposes libuv APIs.

Note: This module is an experiment.

The current implementation is based on [txiki.js](https://github.com/saghul/txiki.js) which is embedded in the [tuv](tuv) folder.

This module is part of the [qjls-dist](https://github.com/javalikescript/qjls-dist) project,
the binaries can be found on the [qjls](http://javalikescript.free.fr/quickjs/) page.

QuickJS libuv is covered by the MIT license.


## Examples

As libuv is based on event loops, the uv calls take callback and require to run the event loop.

```js
import * as uv from 'tuv.so'

const sec = 2;

print('wait for ' + sec + ' seconds');
uv.setTimeout(() => {
  print('timeout reached');
}, sec * 1000);

uv.run();
```
