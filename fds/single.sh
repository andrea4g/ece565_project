#!/bin/bash

file="hal.txt"
fname=$(basename $file)
fbname=${fname%.*}

rm res/${fbname}_result.txt
rm res/${fbname}_mapping.txt
rm res/${fbname}_bind_alloc_result.txt

./bin/fds $file res/ 1.5 -1
echo "Finished $file"
