#!/bin/bash

# TO USE:
# Add fflush(stdout); and abort(); to Window.cpp to get 1 frame output
# Add Vulkan GL_EXT_debug_printf extension and debugPrintfEXT to raygen shader
# Run vkconfig and enable debug printf

rm images/image_$1.txt;
echo "Creating image RTV$1" > ./images/image_$1.txt;

for i in $(seq 0 20 240)
do
    ./build_linux.sh;
    cd build/linux/bin;
    ls;
    ./RayTracer --scene $1 --width 256 --height 256 | tee -a ../../../images/image_$1.txt;
    cd ../../../;
    echo "\n" >> ./images/image_$1.txt;
    sed -i "s/gl_LaunchIDEXT.x >= $i /gl_LaunchIDEXT.x >= $(($i+20)) /g" ./assets/shaders/RayTracing.rgen;
    sed -i "s/gl_LaunchIDEXT.x < $(($i+20)))/gl_LaunchIDEXT.x < $(($i+40)))/g" ./assets/shaders/RayTracing.rgen;
done

sed -i "s/gl_LaunchIDEXT.x >= 260 /gl_LaunchIDEXT.x >= 0 /g" ./assets/shaders/RayTracing.rgen;
sed -i "s/gl_LaunchIDEXT.x < 280)/gl_LaunchIDEXT.x < 20)/g" ./assets/shaders/RayTracing.rgen;

sed -i '/rgba/!d' ./images/image_$1.txt;
