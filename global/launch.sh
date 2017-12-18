#!/bin/bash

rm res/*.txt
rm time/*.txt
rm sch/*.txt
rm data/*.txt

filesInCurrentDir=`ls *.txt`
for file in $filesInCurrentDir; do
  fname=$(basename $file)
  fbname=${fname%.*}
  ./bin/fds $file res/ 3 -1
  ./bin/furegglobal $file res/${fbname}_result.txt
  echo "Finished $file"
done

