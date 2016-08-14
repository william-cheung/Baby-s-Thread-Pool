#include <iostream>

#include "thread_pool.h"

int count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void increment() {
	for (int i = 0; i < 10000; ++i) {
		pthread_mutex_lock(&mutex);
		++count;
		std::cout << count << std::endl;
		pthread_mutex_unlock(&mutex);
	}
	ThreadPool::get_instance()->shutdown();
}

int main() {
	ThreadPool *pool = ThreadPool::get_instance();
	for (int i = 0; i < 2; ++i)
		pool->submit(new Task(increment));
	pool->wait_for_stop();
	pool->join_all();
	return 0;
}
