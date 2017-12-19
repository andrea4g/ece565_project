#!/bin/bash

rm res/*.txt
rm latex.txt

./bin/main invert_matrix_general_dfg_3.txt res/ 3 -1
