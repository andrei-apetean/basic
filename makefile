program: ./bin ./bin_obj
	gcc -o ./bin/program ./src/playground/program.c ./src/basic/basic.c -I./include \
		-I$(VULKAN_SDK)/include -L$(VULKAN_SDK)/lib -lvulkan-1 -ggdb -std=c11 \
		-Wall -Wextra -pedantic

./bin ./bin_obj:
	mkdir $@

run: program
	./bin/program.exe
