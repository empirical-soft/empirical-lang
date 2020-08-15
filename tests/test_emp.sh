#!/bin/bash

# Test whether all Empirical files produce their expected output
# (Must pass-in path to Empirical)

ret=0

for f in *.emp
do
  result=$(diff <($1 --test-mode $f 2>&1) <(grep "##" $f | sed "s/##//"))
  if [[ $? -ne 0 ]]
  then
    echo $f
    echo $result
    echo
    ret=1
  fi
done

for f in $(cd expected/; ls)
do
  result=$(diff $f expected/$f 2>&1)
  if [[ $? -ne 0 ]]
  then
    echo $f
    echo $result
    echo
    ret=1
  fi
done

exit $ret

