- Anything involving chunks is 32x32x32, not 16x16 like minecraft is.
- whenever you make a change to a shader,
  make sure to touch it to force recompilation. Or
  you can always do:
  touch shaders/player_cube.* && cmake --build build