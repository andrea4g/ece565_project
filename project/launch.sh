#!/bin/bash

rm res/*.txt
rm res/*.txtbind
rm res/*.txtfubind
#rm latex.txt

filesInCurrentDir=`ls *.txt`
for file in $filesInCurrentDir; do
  fname=$(basename $file)
  fbname=${fname%.*}
  ./bin/main $file res/ 3 -1
  echo "Finished $file"
done

