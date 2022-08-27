#!/bin/bash


if [ ! -d build ]
    then
    mkdir ./build
fi


if [ $1 ]
  then
  echo "loading config file $1"
  if [ -f $1 ] && [ -r $1 ]
    then
    export $(xargs < $1)
    if [ $1 = *"conf1.env" ]
        then
        make conf1
    else
      if [ $1 = *"conf2.env" ]
        then
        make conf2
      else
        if [ $1 = *"conf3.env" ]
          then
          make conf3
        else
          echo "making custom config"
          make run.out
        fi
      fi
    fi
  else
    echo "argument $1 is not a file or you don't have read permission"
  fi
else
  echo "no config file specified. Usage . ./load_config <file>"
fi