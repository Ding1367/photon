# photon
An extremely minimal and lightweight editor.

I'm still working on the editor so there's a lot of debug stuff.

# Build steps
1.  Install CMake
2.  Make a build directory and go in it
    ```sh
    mkdir build
    cd build
    ```
3.  Run the following:
    ```sh
    cmake -S .. -B . # builds as Debug by default, use -DCMAKE_BUILD_TYPE=Release to not build with debug options
    cmake --build .
    ```

That's it!

# How to debug
When building a Debug build, you can use `C-s` in the editor to take a snapshot which will, for the frame, generate a back and front snapshot, which is just the framebuffers. You can use `tools/diff.py` to compare them.

You can also, when generating build files, provide `USER_DEBUG_OPTIONS` as a comma separated list to enable file logs for certain things. For each option, you have to define them as a path to the log file.

* `UI_DEBUG_REFRESH`: if set, refresh logic will be logged to the path.
* `UI_DEBUG_CALLS`: if set, calls to `photon_ui_draw_*` will be printed.

# How to write extensions
See [HACKING.md](HACKING.md)
