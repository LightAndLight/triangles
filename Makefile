app:
	clang++ --std=c++11 -lvulkan -lglfw -O src/main.cpp -o app

install:
	mkdir -p $(out)/bin
	cp app $(out)/bin
