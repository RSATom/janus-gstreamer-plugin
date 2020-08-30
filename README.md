[![Snap Status](https://build.snapcraft.io/badge/RSATom/janus-gstreamer-plugin.svg)](https://build.snapcraft.io/user/RSATom/janus-gstreamer-plugin)

# janus-gstreamer-plugin
GStreamer plugin for Janus Gateway

## Install and use as snap package
* Install: `sudo snap install janus-plus-gstreamer-plugin --edge`
* Configs location: `/var/snap/janus-plus-gstreamer-plugin/common/etc`
* Restart (required after configs edit): `sudo snap restart janus-plus-gstreamer-plugin`

## Build from sources
* Build and install Janus Gateway as described at https://github.com/meetecho/janus-gateway#compile
* `git clone https://github.com/RSATom/janus-gstreamer-plugin.git --recursive && cd janus-gstreamer-plugin`
* `autogen.sh`
* `./configure --prefix=/path/to/installed/janus`
* `make && make install`
