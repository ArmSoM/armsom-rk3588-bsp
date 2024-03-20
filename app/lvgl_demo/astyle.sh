#!/bin/bash
#
array=$(find . -type f | grep -E "\.c$|\.h$")

for name in ${array}
do
    chmod 644 ${name}
    astyle --style=allman --indent=spaces=4 --pad-oper --pad-header --unpad-paren --suffix=none --align-pointer=name --lineend=linux --convert-tabs --verbose ${name}
done

