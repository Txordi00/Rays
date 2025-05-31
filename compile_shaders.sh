#!/bin/bash
root_path=$(dirname $0)
shaders_path="${root_path}/shaders"
comp_shaders_path="."
mkdir -p ${comp_shaders_path}
frag_shaders=$(find $root_path -type f -name "*.frag")
vert_shaders=$(find $root_path -type f -name "*.vert")
all_shaders="$frag_shaders $vert_shaders"
for i in $all_shaders
do
        fn=$(basename $i)
	out_fn="${comp_shaders_path}/${fn}.spv"
	glslc $i -o $out_fn
	echo "[GLSL] shader $(realpath ${out_fn}) compiled"
done
