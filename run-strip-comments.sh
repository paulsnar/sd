#!/bin/sh

file="$1"
sed -e 's/;.*$//g' -e 's/ //g' "$file" | tr -d '\n' | python sd.py
