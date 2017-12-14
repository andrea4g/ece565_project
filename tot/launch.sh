#!/bin/bash

rm res/*.txt
rm time/*.txt
rm sch/*.txt

filesInCurrentDir=`ls *.txt`
for file in $filesInCurrentDir; do
  fname=$(basename $file)
  fbname=${fname%.*}
  ./fds $file res/ 3 -1
  ./fureg $file res/${fbname}_result.txt
  echo "Finished $file"
done

