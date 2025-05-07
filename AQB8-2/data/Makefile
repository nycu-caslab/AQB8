all: generate
generate: generate.cpp
	g++ generate.cpp -O3 -Wall -I./bvh-quantize/src/bvh/include -I./bvh-quantize/src/happly -I./bvh-quantize/src -I./bvh-quantize -I. -fopenmp -o generate
clean:
	rm -f generate gen_nbp.bin gen_cluster.bin trig.bin ray.bin result.bin
