/**
 *  Baby's Thread Pool
 *
 *  William Cheung, 08/13/2015
 *
 */


#include <queue>

#include <pthread.h>
#include <assert.h>


class Task {
	void (*_runnable)(void);
public:
	explicit Task(void (* fun_ptr)(void)): _runnable(fun_ptr) { }
	void run() {
		if (_runnable != NULL)
			_runnable();
	}
};

#define NUM_CPU_CORES  4

/* A pthread based thread pool */

class ThreadPool {
private:
	class TaskQueue {	
	public:
		TaskQueue(ThreadPool* pool): _pool(pool) {
			pthread_mutex_init(&_mutex, NULL);
			pthread_cond_init(&_cond, NULL);
		}

		~TaskQueue() {
			pthread_mutex_destroy(&_mutex);
			pthread_cond_destroy(&_cond);
		}
		
		Task *next_task() {
			Task *next = NULL;
			pthread_mutex_lock(&_mutex);
			
			while (!_pool->is_shutdown() && _tasks.empty())
				pthread_cond_wait(&_cond, &_mutex);
			
			if (_pool->is_shutdown())
				return NULL;
			
			next = _tasks.front();
			_tasks.pop();
			
			pthread_mutex_unlock(&_mutex);
			return next;
		}
		
		void add_task(Task *task) {
			pthread_mutex_lock(&_mutex);
			_tasks.push(task);
			pthread_cond_signal(&_cond);
			pthread_mutex_unlock(&_mutex);
		}

		bool closed() {
			return _pool->is_shutdown();
		}
	
	private:
		ThreadPool* _pool; 

		std::queue<Task*> _tasks; // the actural task queue
		pthread_mutex_t _mutex;
		pthread_cond_t _cond;
	};

public:
	static ThreadPool* get_instance() {
		static ThreadPool pool(NUM_CPU_CORES);
		return &pool;
	}

	void submit(Task *task) {
		_task_queue.add_task(task);
	}

	void shutdown() {
		_is_shutdown = true;
	}

	bool is_shutdown() {
		return _is_shutdown;
	}

	void join_all() {
		for (int i = 0; i < _nthreads; ++i)
			pthread_join(_threads[i], NULL);
	}

	void wait_for_stop() {
		while (!_is_shutdown) 
			;
	}

private:
	ThreadPool(int nthreads): 
		_nthreads(nthreads), _is_shutdown(false), _task_queue(this) {
		_threads = new pthread_t[nthreads];
		for (int i = 0; i < nthreads; ++i) 
			assert(pthread_create(&_threads[i], 
			                      NULL, 
			                      run_task, &_task_queue) == 0);
	}

	static void* run_task(void *arg) {
		TaskQueue *task_queue = (TaskQueue *) arg;
		while (!task_queue->closed()) { // test if the pool is shutdown
			// look for a task to execute
			Task *next = task_queue->next_task();
			if (next != NULL) { 
				next->run(); 
				delete next; 
			}
		}
	}

	pthread_t *_threads;
	int _nthreads;
	TaskQueue _task_queue;
	bool _is_shutdown;
};

