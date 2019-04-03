{ stdenv, clang, vulkan-headers, vulkan-loader
, vulkan-validation-layers, glfw
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
    ];
  }
