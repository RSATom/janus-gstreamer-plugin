[![Snap Status](https://build.snapcraft.io/badge/RSATom/janus-gstreamer-plugin.svg)](https://build.snapcraft.io/user/RSATom/janus-gstreamer-plugin)

# janus-gstreamer-plugin
GStreamer plugin for Janus Gateway

## Build
* Build and install Janus Gateway as described at https://github.com/meetecho/janus-gateway#compile
* `git clone https://github.com/RSATom/janus-gstreamer-plugin.git --recursive && cd janus-gstreamer-plugin`
* `autogen.sh`
* `./configure --prefix=/path/to/installed/janus`
* `make && make install`
