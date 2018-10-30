#!/bin/sh

#git pull

ops="--jobs=2 tcmalloc=1 $1 $2 $3 $4 "

scons $ops
if [ $? = 0 ]; then
  cd netraid && scons $ops
  cd ..
else
  echo "error occured"
fi

