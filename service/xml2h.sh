#!/bin/sh
#  Copyright (C) 2016 Johan Thelin <e8johan@gmail.com>
#  Licensed under GPLv2, see file LICENSE in this source tree.
#
# Usage: xml2h <variable> <input>

echo -n "const char $1[] = {"
hexdump -ve '1/1 "0x%x,"' $2
echo "0x00};"
