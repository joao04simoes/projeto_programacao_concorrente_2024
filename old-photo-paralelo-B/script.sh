#!/bin/bash

datasets=("Dataset-X")
threads=(1 2 4 8 16 32)

for dataset in "${datasets[@]}"; do
  
  for thread in "${threads[@]}"; do
    rm -r ../$dataset/old_photo_PAR_B/
    ./old-photo-paralelo-B "../$dataset" "$thread"

  done

done
