#!/bin/bash

#Lute Lillo Portero -- Homework 2 -- Shell

existsShell="lute_shell"
if [ -f "$existsShell" ]; then
    make clean
fi

make
touch copy.txt

#Now we are going to run 20 tests to check the functionality

./lute_shell 


