Compiling and Running
---------------------

Use CMake:
```sh
cmake -B build
cmake --build build
./build/lsh
```

The starter code has been tested on:
- Ubuntu 22.04
- OpenSuse Tumbleweed 20260829 (see note below)
- Debian 6.1.94-1 (StuDAT)
- macOS 12.x / 13.x (Apple silicon)

Local Development Setup
-----------------------

If you choose to develop locally, ensure that you have the necessary packages installed: a compiler, CMake, Readline, and ncurses (which includes termcap functionality).
For example, on Ubuntu, you can install them using:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake libreadline-dev libncurses5-dev libncursesw5-dev
```

Troubleshooting Building
------------------------

On some newer Linux distributions (e.g. OpenSuse Tumbleweed 20260829), you may get a linker error with the provided CMakeLists.txt. This is because now the termcap functionality should be linked as `ncurses`, rather than using the `termcap` library directly (which in some cases no longer exists). The fix here is to change the following line:

```
target_link_libraries(lsh PRIVATE readline termcap)
```

to this:

```
target_link_libraries(lsh PRIVATE readline ncurses)
```
