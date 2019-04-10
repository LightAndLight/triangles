debug: src/main.cpp shaders/vert.spv shaders/frag.spv
	clang++ --std=c++11 -lvulkan -lglfw -O0 -g src/main.cpp -o app

app: src/main.cpp shaders/vert.spv shaders/frag.spv
	clang++ --std=c++11 -lvulkan -lglfw src/main.cpp -o app

shaders/vert.spv shaders/frag.spv: shaders/triangle.vert shaders/triangle.frag
	glslangValidator -V shaders/triangle.vert -o shaders/vert.spv
	glslangValidator -V shaders/triangle.frag -o shaders/frag.spv

install:
	mkdir -p $(out)/bin
	cp app $(out)/bin
