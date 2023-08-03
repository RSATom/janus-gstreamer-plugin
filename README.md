[![Snap Status](https://snapcraft.io/static/images/badges/en/snap-store-white.svg)](https://snapcraft.io/janus-plus-gstreamer-plugin)

# janus-gstreamer-plugin
GStreamer plugin for Janus Gateway

## Install and use as snap package
* Install: `sudo snap install janus-plus-gstreamer-plugin --edge`
* Configs location: `/var/snap/janus-plus-gstreamer-plugin/common/etc`
* Restart (required after configs edit): `sudo snap restart janus-plus-gstreamer-plugin`

## Build from sources
* Build and install Janus Gateway as described at https://github.com/meetecho/janus-gateway#compile
* `git clone https://github.com/RSATom/janus-gstreamer-plugin.git --recursive`
* `mkdir -p ./janus-gstreamer-plugin-build`
* `cd ./janus-gstreamer-plugin-build && cmake ../janus-gstreamer-plugin && make && make install`
