/*
 * This is a benchmark to test allocators in producer-consumer pattern.
 * Input $nthread$ must be even.
 * 
 * The benchmark is designed per the description of prod-con in the paper:
 *    Makalu: Fast Recoverable Allocation of Non-volatile Memory
 *    K. Bhandari et al.
 */

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "fred.h"
#include "cpuinfo.h"
#include "timer.h"

#include "MichaelScottQueue.hpp"

#include "AllocatorMacro.hpp"

// This class holds arguments to each thread.
class workerArg {
public:

  workerArg() {}

  workerArg (MichaelScottQueue<char*>* _msq, int _objNum, int _objSize)
    : msq (_msq),
      objNum (_objNum),
      objSize (_objSize)
  {}

  MichaelScottQueue<char*>* msq;
  int objNum;
  int objSize;
};
pthread_barrier_t barrier;
void * producer (void * arg)
{
	// Producer: allocate objNum of objects in size of ObjSize, push each to msq
	workerArg& w1 = *(workerArg *) arg;
	pthread_barrier_wait(&barrier);
	for (int i = 0; i < w1.objNum; i++) {
		// Allocate the object.
		char * obj = (char*)pm_malloc(sizeof(char)*w1.objSize);
		// Write into it
		for (int k = 0; k < w1.objSize; k++) {
			obj[k] = (char) k;
			volatile char ch = obj[k];
			ch++;
		}
		// push to msq
		w1.msq->enqueue(obj, 0);
	}
	return NULL;
}

void * consumer (void * arg)
{
	// Consumer: pop objects from msq, deallocate objNum of objects
	workerArg& w1 = *(workerArg *) arg;
	int i = 0;
	pthread_barrier_wait(&barrier);
	while(i < w1.objNum) {
		// pop from msq
		auto obj = w1.msq->dequeue(1);
		if(obj) {
			// deallocate it if not null
			pm_free(obj.value());
			i++;
		}
	}
	return NULL;
}

int main (int argc, char * argv[]){
	int nthreads;
	int objNum = 10000000;
	int objSize = 64; // byte

	if (argc > 3) {
		nthreads = atoi(argv[1]);
		objNum = atoi(argv[2]);
		objSize = atoi(argv[3]);
		if(nthreads%2!=0) {
			fprintf (stderr, "nthreads must be even\n");
			return 1;
		}
	} else {
		fprintf (stderr, "Usage: %s nthreads objNum objSize\n", argv[0]);
		return 1;
	}
	pthread_barrier_init(&barrier,NULL,nthreads);
	HL::Fred * threads = new HL::Fred[nthreads];
	HL::Fred::setConcurrency (HL::CPUInfo::getNumProcessors());
	std::vector<MichaelScottQueue<char*>*> msqs;
	std::vector<workerArg> wArg;
	int i;
	for (i = 0; i < nthreads/2; i++) {
		msqs.emplace_back(new MichaelScottQueue<char*>(2));
		wArg.emplace_back(msqs[i], objNum*2/nthreads, objSize);
		wArg.emplace_back(msqs[i], objNum*2/nthreads, objSize);
	}

	pm_init();
	HL::Timer t;
	t.start();

	for(i = 0; i < nthreads/2; i++) {
		threads[i<<1].create (&producer, (void *) &wArg[i<<1]);
		threads[(i<<1)+1].create (&consumer, (void *) &wArg[(i<<1)+1]);
	}
	for (i = 0; i < nthreads; i++) {
		threads[i].join();
	}

	t.stop();

	for(auto & m:msqs){
		delete m;
	}
	delete [] threads;
	printf ("Time elapsed = %f seconds.\n", (double) t);

	pm_close();
	return 0;
}
