#!/bin/bash

datasets=("Dataset-X")
threads=(1 2 4 8 16 32)

for dataset in "${datasets[@]}"; do
  
  rm -r ../$dataset/old_photo_PIPELINE/
  ./old-photo-pipeline "../$dataset"
done
