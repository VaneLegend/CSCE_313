#!/bin/bash

# Array of file sizes to test (in bytes)
file_sizes=(1000 10000 100000 1000000 10000000 100000000)

# Original file to use as a base
original_file="BIMDC/1.csv"

# Temporary file for truncation
temp_file="temp_file.csv"
bimdc_file="BIMDC/temp_file.csv"

# Loop through file sizes
for size in "${file_sizes[@]}"
do
    echo "Testing file size: $size bytes"
    
    # Copy original file to temp file
    cp "$original_file" "$bimdc_file"
    
    # Truncate the temp file to the desired size
    truncate -s $size "$bimdc_file"
    
    # Run the client with the temp file and capture the output
    output=$(./client -f "temp_file.csv")
    
    echo "$output"
    
    echo "----------------------"
    
    # Clean up the temp file
    rm "$bimdc_file"
done