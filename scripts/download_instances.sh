#!/bin/bash

# Given a file with a list of names, this script will download each at https://gbd.iti.kit.edu/file/$name, extract it using xz and run the provided command

names=$1
cmd=$2

for name in $(cat $names); do
    url="https://gbd.iti.kit.edu/file/$name"

    wget -O $name.xz $url 2> /dev/null
    echo "Downloaded $name"
    xz -d $name.xz

    subst_cmd=$(echo $cmd | sed "s/<file>/$name/g")

    sh -c "$subst_cmd"
    echo "Finished $name"

    rm $name
done
