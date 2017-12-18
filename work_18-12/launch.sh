#!/bin/bash

rm res/*.txt
rm latex.txt

filesInCurrentDir=`ls *.txt`
for file in $filesInCurrentDir; do
  fname=$(basename $file)
  fbname=${fname%.*}
  ./bin/main $file res/ 1.5 -1
  echo "Finished $file"
done

