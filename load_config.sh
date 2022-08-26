#!/bin/bash


if [ $1 ]
  echo "loading config $1"
  then
  if [ $1 = "1" ]
    then
    export $(xargs < conf1.env)
    make conf1
  else
    if [ $1 = "2" ]
      then
      export $(xargs < conf2.env)
      make conf2
    else
      if [ $1 = "3" ]
        then
        export $(xargs < conf3.env)
        make conf3
      else
        echo "invalid config specified. Running make only"
        make
      fi
    fi
  fi
else
  echo "no config specified. Running make only"
  make
fi