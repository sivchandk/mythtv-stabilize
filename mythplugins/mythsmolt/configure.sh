#!/bin/bash

prefix=''
sysroot=''

if [ -e $sysroot$prefix/include/mythtv/mythconfig.mak ]
then
  rm mythconfig.mak 2> /dev/null
  ln -s $sysroot$prefix/include/mythtv/mythconfig.mak mythconfig.mak

elif [ -e /usr/local/include/mythtv/mythconfig.mak ]
then
    prefix="/usr/local"
    rm mythconfig.mak 2> /dev/null
    ln -s $sysroot$prefix/include/mythtv/mythconfig.mak mythconfig.mak

elif [ -e /usr/include/mythtv/mythconfig.mak ]
then
    prefix="/usr"
    rm mythconfig.mak 2> /dev/null
    ln -s $sysroot$prefix/include/mythtv/mythconfig.mak mythconfig.mak
else
  echo "ERROR: mythconfig.mak not found at $sysroot$prefix/include/mythtv/mythconfig.mak"
  echo "Did you make AND install MythTV first?"
  echo "Are you using the correct prefix ($prefix) and sysroot ($sysroot)?"
  echo "Bailing out!!"
  exit
fi
qmake mythsmolt.pro