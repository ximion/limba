#!/bin/sh
set -e
sed -i '/^#/ d' *.po
cd ..
make
cd po/
