#!/bin/bash

#
# Some tests for binmay
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
T3=$(mktemp /tmp/binmaytest.XXXXXX)

BUF_LENGTH=1024

echo "Check files: $T1, $T2"


function buffertest() # {{{
{
	testno="$1"
	buf="$2"

	echo -n "$testno: Buffer length $buf..."
	dd if=/dev/zero of=$T1 bs=1 count=$buf 2>/dev/null || die "dd fail" 
	$BINMAY -s '00' -r 'ff' -i $T1 -o $T2 || die "Program crash"
	if ! diff $T2 testfiles/$testno >/dev/null; then
		die "Mismatch."
	fi
	echo OK
} # }}}
function comp_files() # {{{
{
	f1="$1"
	f2="$2"

	if [ "0" != $(diff $f1 $f2|wc -c) ]; then
		echo "Test failure! File $f1 differs from $f2"
		echo "Output: "
		hexdump -C $f1
		echo "Should be: "
		hexdump -C $f2
		exit 1
	else
		echo "OK "
	fi
} # }}}
function check() # {{{
{
	comp_files $T1 $T2
#	if [ "0" != $(diff $T1 $T2|wc -c) ]; then
#		echo "Test failure!"
#		echo "Output: "
#		hexdump -C $T1
#		echo "Should be: "
#		hexdump -C $T2
#		exit 1
#	else
#		echo "OK "
#	fi
} # }}}
function die() # {{{
{
	echo $*
	exit 1
} # }}}

echo -n "1: Deletion..."
$BINMAY -p "aa fa ff fb ff"|$BINMAY -s "fa ff fb ff" -r "" -o $T1 || die "Program crash"
$BINMAY -p "aa " >$T2 || die "Program crash"
check

echo -n "2: Masked search..."
$BINMAY -p "fa ff fa ff fb ff fb ff"|$BINMAY -s "fa ff fa ff" -S "f0 ff f0 ff" -r "ff" -o $T1 || die "Program crash"
$BINMAY -p "ff ff" >$T2 || die "Program crash"
check

echo -n "3: Masked search and replace..."
$BINMAY -p "fa 0f fa ff fb ff fb ff"|$BINMAY -s "fa ff fa ff" -S "f0 0f f0 ff" -r "ff ff aa aa"  -R "00 7f ff 00" -o $T1 || die "Program crash"
$BINMAY -p "fa 7f aa ff fb ff aa ff" >$T2 || die "Program crash"
check

echo -n "4: Masked search and replace w/ replacement > rmask..."
$BINMAY -p "fa 0f fa ff fb ff fb ff"|$BINMAY -s "fa ff fa ff" -S "f0 0f f0 ff" -r "ff ff aa aa 00 01 02 03 04 05 06"  -R "00 7f ff 00" >$T1 || die "Program crash"
$BINMAY -p "fa 7f aa ff 00 01 02 03 04 05 06 fb ff aa ff 00 01 02 03 04 05 06" >$T2 || die "Program crash"
check

INBIN="1100111111001111"
OUTBIN="0101010101010101"

echo -n "5: Binary input test..."
$BINMAY -bp $INBIN|$BINMAY -b -s $INBIN -r $OUTBIN >$T1 || die "Program crash"
$BINMAY -bp $OUTBIN >$T2 || die "Program crash"
check

echo -n "6: Partial match..."
$BINMAY -p "61 61 62"|$BINMAY -s "61 62" -r "62 63" -o $T1 || die "Program crash"
$BINMAY -p "61 62 63 " >$T2 || die "Program crash"
check

buffertest 7 1024
buffertest 8 1025
buffertest 9 1023

buffertest 10 2048
buffertest 11 2047
buffertest 12 2049

echo -n "13: Search test..."
echo "The time has come for all good men to come to the aid of the party"| \
	$BINMAY -s '60' -S 'f0' >$T1 || die "Program crash"
comp_files $T1 testfiles/searchtest

echo -n "14: Format prefix h test..."
echo "bark bark barrrrk"| $BINMAY -s "h:62" -r "h:78"| grep "xark xark xarrrrk" >/dev/null || die "fail"; 
echo "OK"

echo -n "15: Format prefix b test..."
echo "bark bark barrrrk"| $BINMAY -s "b:01100010" -r "b:01111000" | grep "xark xark xarrrrk" >/dev/null || die "fail"; 
echo "OK"

echo -n "16: Format prefix b test..."
$BINMAY -p "b:01100011 01100001 01110100"|grep "^cat$" >/dev/null || die "fail";
echo "OK"

echo -n "17: Format prefix f test..."
$BINMAY -p "f:testfiles/puketest"|grep "^This is the puke test$" >/dev/null || die "fail"
echo "OK"

rm -f $T1 $T2 $T3
