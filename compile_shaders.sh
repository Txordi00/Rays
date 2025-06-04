#!/bin/bash
root_path=$(dirname $0)
shaders_path="${root_path}/shaders"
compiled_shaders_path="."
mkdir -p ${compiled_shaders_path}
frag_shaders=$(find $shaders_path -type f -name "*.frag")
vert_shaders=$(find $shaders_path -type f -name "*.vert")
comp_shaders=$(find $shaders_path -type f -name "*.comp")
all_shaders="$frag_shaders $vert_shaders $comp_shaders"
for i in $all_shaders
do
        fn=$(basename $i)
	out_fn="${compiled_shaders_path}/${fn}.spv"
	glslc $i -o $out_fn
	echo "[GLSL] shader $(realpath ${out_fn}) compiled"
done
