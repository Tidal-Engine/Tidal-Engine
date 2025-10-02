#!/bin/bash

glslc cube.vert -o cube_vert.spv
glslc cube.frag -o cube_frag.spv

echo "Shaders compiled successfully"
