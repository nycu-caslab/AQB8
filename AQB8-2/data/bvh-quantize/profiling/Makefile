T_TRV_INT ?= 0.5
T_SWITCH ?= 1

all:
	make vanilla.target
	make quantized.target

%.target: %.out
	if [ -z ${SCENE} ]; then false; fi
	bash setup_and_perf.sh $< ${SCENE} ${T_TRV_INT} ${T_SWITCH}

%.out: %.cpp
	g++ -w --std=c++17 -O3 -I../src/bvh/include -DNDEBUG $< -o $@

clean:
	rm -rf vanilla.out quantized.out perf.data perf.data.old
	if [ -p ctl.fifo ]; then unlink ctl.fifo; fi
	if [ -p ack.fifo ]; then unlink ack.fifo; fi
