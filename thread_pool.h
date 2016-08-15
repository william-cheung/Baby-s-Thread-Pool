/**
 *  Baby's Thread Pool
 *
 *  William Cheung, 08/13/2015
 *
 */


#include <queue>       // for std::queue

#include <unistd.h>    // for sleep()

#include <pthread.h>   // for pthread_*
#include <assert.h>    // for assert()
   

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
	// TaskQueue: a blocking queue implementation w/o limit on its size.
	// 
	// 'add_task' enqueues tasks to the queue
	// 'next_task' dequeues tasks from the queue
	//
	// A thread trying to dequeue from an empty queue will be blocked 
	// (using pthread_cond_wait) until some other thread submits a task 
	// to the queue.
	//
	// Closing a TaskQueue will clear all tasks in the queue. If a queue is 
	// closed, 'add_task' does nothing and 'next_task' returns NULL.
	// 
	// Thread safety: after a TaskQueue is initialized (or constructed), 
	// it is thread-safe.
	//
	class TaskQueue {	
	public:
		TaskQueue(): _closed(false) {
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
			
			while (!_closed && _tasks.empty())
				pthread_cond_wait(&_cond, &_mutex);
			
			// if we are here, the queue is closed 
			// or it is not empty.

			if (_closed) {
				pthread_mutex_unlock(&_mutex);
				return NULL;
			}

			next = _tasks.front();
			_tasks.pop();
			
			pthread_mutex_unlock(&_mutex);
			return next;
		}
		
		bool add_task(Task *task) {
			pthread_mutex_lock(&_mutex);
			if (!_closed) { 
				_tasks.push(task);
				pthread_cond_signal(&_cond);
			} // else if _closed == true, noop
			pthread_mutex_unlock(&_mutex);
			return !_closed;
		}

		void close() {
			// take notice of the "lost wake-up" problem
			pthread_mutex_lock(&_mutex);
			
			// clear tasks in the queue
			while (!_tasks.empty()) {
				Task *task = _tasks.front();
				_tasks.pop();
				delete task;
			}
			
			// set the queue closed and wake up all threads 
			// waiting on '_cond'
			_closed = true;
			pthread_cond_broadcast(&_cond);
			
			pthread_mutex_unlock(&_mutex);
		}

		bool closed() {
			return _closed;
		}
	
	private:
		std::queue<Task*> _tasks; // the actural task queue
		pthread_mutex_t _mutex;
		pthread_cond_t _cond;
		volatile bool _closed;    // TODO: is 'volatile' necessary here?
	};

public:
	// All public methods are thread-safe

	static ThreadPool* get_instance() {
		static ThreadPool pool(NUM_CPU_CORES);
		return &pool;
	}


	// Submit a task to the thread pool. if the pool is shutdown, noop
	//    usage: submit(new Task(...))
	//    returns: true on success, false on fail
	bool submit(Task *task) {
		return _task_queue.add_task(task);
	}

	// Shutdown the thread pool. after the pool is shutdown, it can't
	// accept any tasks and no tasks will be executed except the running 
	// ones.
	void shutdown() {
		_task_queue.close();
	}

	bool is_shutdown() {
		return _task_queue.closed();
	}
	
	// Wait all threads to terminate. 
	void join_all() {
		for (int i = 0; i < _nthreads; ++i)
			pthread_join(_threads[i], NULL);
	}
	
	// Wait until some other thread calls shutdown()
	void wait_for_stop() {
		while (!_task_queue.closed()) {
			// relinquish the processor for other threads to run
			sleep(1); // TODO: sleep for a shorter period of time
		}
	}

private:
	ThreadPool(int nthreads): _nthreads(nthreads) {
		_threads = new pthread_t[nthreads];
		for (int i = 0; i < nthreads; ++i) {
			assert(pthread_create(&_threads[i], 
			                      NULL, 
			                      run_task, &_task_queue) == 0);
			// pthread_detach(_threads[i]);
		}
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
};

