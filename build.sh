#!/bin/bash

dirs=( lib/formats/gfx/prs_converter lib/formats/gfx/spb_converter lib/formats/gfx/g00_converter )

for x in "${dirs[@]}"; do
    pushd "$x"
    ruby extconfig.rb && make
    popd
done