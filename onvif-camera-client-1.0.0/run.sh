#!/bin/bash
DIR=$(dirname "$(readlink -f "$0")")
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$DIR/plugins"
export QT_QPA_PLATFORM=xcb
exec "$DIR/bin/onvif-client" "$@"
