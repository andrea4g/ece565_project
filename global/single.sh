#!/bin/bash

file="hal.txt"
fname=$(basename $file)
fbname=${fname%.*}

rm res/${fbname}_result.txt
rm res/${fbname}_mapping.txt
rm data/${fbname}.txt
rm res/${fbname}_bind_alloc_result.txt
rm time/${fbname}_time.txt
rm sch/${fbname}_sch.txt

./bin/fds $file res/ 1.5 -1
./bin/furegglobal $file res/${fbname}_result.txt
echo "Finished $file"
