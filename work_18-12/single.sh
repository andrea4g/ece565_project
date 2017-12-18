#!/bin/bash

rm res/*.txt
rm latex.txt

./bin/main interpolate_aux_dfg_12.txt res/ 1.5 -1
