#!/bin/bash

folder="atsp";
ext="tsp";
q=0;
while read line
do
  File[ $q ]="${line%.*}";
  n[ $q ]="${filename[ $i ]//[!0-9]/}";
  (( q++ ));
done < <(ls atsp/)

lenFilename=${#File[@]}

Kprocs=('0' '16' '32' '64' '100' '128');

a="0";
#for i in {1..8}; do
for (( i=1; i<=$lenFilename; i++))
do
  Kprocs="4";
#  echo ${str}
#  echo ${lenFilename}
#  echo ${a}
#  echo ${i}
  for j in {1..5}; do
    sed "s/Filename/${File[a]}/g" pref_RANDOM_POLLING.pbs > "test_${File[a]}_${Kprocs[j]}.pbs";
    sed -i "s/ext/${ext}/g" "test_${File[a]}_${Kprocs[j]}.pbs"
    sed -i "s/folder/${folder}/g" "test_${File[a]}_${Kprocs[j]}.pbs"
    sed -i "s/Kprocs/${Kprocs[j]}/g" "test_${File[a]}_${Kprocs[j]}.pbs"
  done
  a="$(( $a + 1 ))";
done
