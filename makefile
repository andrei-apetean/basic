program: ./bin ./bin_obj
	gcc -o ./bin/program ./src/playground/program.c -I./include -I$(VULKAN_SDK)/include -L$(VULKAN_SDK)/lib -lvulkan-1 -ggdb

./bin ./bin_obj:
	mkdir $@

run: program
	./bin/program.exe
