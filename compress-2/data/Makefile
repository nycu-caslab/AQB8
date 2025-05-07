all: generate
generate: generate.cpp
	g++ generate.cpp -O3 -Wall -I./bvh/include -I./happly -I. -fopenmp -o generate
clean:
	rm -f generate gen_ref_bbox.bin gen_nbp.bin trig.bin ray.bin result.bin