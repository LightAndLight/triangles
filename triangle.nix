{ stdenv, clang, vulkan-headers, vulkan-loader
, vulkan-validation-layers, glfw, glslang
}:
stdenv.mkDerivation {
    name = "triangle";
    src = ./.;
    buildInputs = [ 
      clang 
      vulkan-headers 
      vulkan-loader
      vulkan-validation-layers
      glfw
      glslang
    ];
  }
