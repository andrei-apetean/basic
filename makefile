program:
	gcc -o program ./src/playground/program.c -I./include -I$(VULKAN_SDK)/include -L$(VULKAN_SDK)/lib -lvulkan-1
