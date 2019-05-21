#!/bin/bash

# Test whether all VVM files produce their expected output
# (Must pass-in path to Empirical)

ret=0
for f in *.vvm
do
  result=$(diff <($1 $f) <(grep ";;" $f | sed "s/;;//"))
  if [[ $? -ne 0 ]]
  then
    echo $f
    echo $result
    ret=1
  fi
done
exit $ret

