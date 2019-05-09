#!/bin/bash

#
# Some lame tests for binmay
#

BINMAY=./binmay

#echo "Should \"Done: 1 matches\""
#$BINMAY -p "ff ff ff"|$BINMAY -s "ff ff ff" >/dev/null
#
#
#echo "Should \"Done: 1 matches\""
#$BINMAY -p "ff ff ff ff ff ff"|$BINMAY -s "ff ff ff" >/dev/null
#

T1=$(mktemp /tmp/binmaytest.XXXXXX)
T2=$(mktemp /tmp/binmaytest.XXXXXX)

echo "Check files: $T1, $T2"

check()
{
  if [ "0" != $(diff $T1 $T2|wc -c) ]; then
    echo "Test failure!"
    echo "Output: "
    hexdump -C $T1
    echo "Should be: "
    hexdump -C $T2
    exit 1
  else
    echo "OK "
  fi
}


echo -n "1: Deletion..."
$BINMAY -p "aa fa ff fb ff"|$BINMAY -s "fa ff fb ff" -r "" -o $T1
$BINMAY -p "aa " >$T2
check

echo -n "2: Masked search..."
$BINMAY -p "fa ff fa ff fb ff fb ff"|$BINMAY -s "fa ff fa ff" -S "f0 ff f0 ff" -r "ff" -o $T1
$BINMAY -p "ff ff" >$T2
check

echo -n "3: Masked search and replace..."
$BINMAY -p "fa 0f fa ff fb ff fb ff"|$BINMAY -s "fa ff fa ff" -S "f0 0f f0 ff" -r "ff ff aa aa"  -R "00 7f ff 00" -o $T1
$BINMAY -p "fa 7f aa ff fb ff aa ff" >$T2
check

echo -n "4: Masked search and replace w/ replacement > rmask..."
$BINMAY -p "fa 0f fa ff fb ff fb ff"|$BINMAY -s "fa ff fa ff" -S "f0 0f f0 ff" -r "ff ff aa aa 00 01 02 03 04 05 06"  -R "00 7f ff 00" >$T1
$BINMAY -p "fa 7f aa ff 00 01 02 03 04 05 06 fb ff aa ff 00 01 02 03 04 05 06" >$T2
check

INBIN="1100111111001111"
OUTBIN="0101010101010101"

echo -n "5: Binary input test..."
$BINMAY -bp $INBIN|$BINMAY -b -s $INBIN -r $OUTBIN >$T1
$BINMAY -bp $OUTBIN >$T2
check

rm -f $T1 $T2
