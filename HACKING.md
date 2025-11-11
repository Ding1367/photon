# Hacking `photon`
1.  Create the extensions directory, at `~/.config/photon/extensions`
2.  Write an extension in C or a language that can output C symbols. More info in [The ABI](#the-abi).
3.  Compile your extension as a dynamic library and move to the extensions directory. (You can disable it with -- prefix).

## The ABI
All ABI functions take in a `const photon_api_t *` parameter. To get access to types include `src/photon.h` in your code.

Here are all the ABI functions:
* `photon_on_load`: called when the extension is loaded
* `photon_on_unload`: called when the extension is unloaded
* `photon_pre_frame`: called before a frame is rendered if your extension adds any UI to the screen. This can be ignored for now, as UI isn't exactly polished.

# Hooks
You can set your extension's hooks at `api->hooks`. A hook has the signature: `void(const photon_api_t *, photon_event_t *)`.

Some events can be cancelled. To cancel an event, set `event->cancelled` to a truthy value, or falsy to uncancel it if it was cancelled.
Some events also have data, like a key press event or a buffer created event. You can access the data in a few ways.

For buffer related events, access `event->buffer`, and for other events access `event->data`.

The only hooks available are `on_keypress` and `on_new_buf`.
