#!/bin/bash

rm res/*.txt
rm time/*.txt
rm sch/*.txt

filesInCurrentDir=`ls *.txt`
for file in $filesInCurrentDir; do
  fname=$(basename $file)
  fbname=${fname%.*}
  ./bin/fds $file res/ 1.5 -1
  ./bin/fureg $file res/${fbname}_result.txt
  echo "Finished $file"
done

