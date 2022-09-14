#!/bin/sh

echo "#pragma once">$1
echo -n "const char* const APP_VERSION_STR = \"">>$1
cat version.txt>>$1
echo "\";">>$1