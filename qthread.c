/*
 * file:        qthread.c
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Spring 2019
 */

/* a bunch of includes which will be useful */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include "qthread.h"



/* prototypes for stack.c and switch.s
 * see source files for additional details
 */
extern void switch_to(void **location_for_old_sp, void *new_value);
extern void *setup_stack(long *stack, void *func, void *arg1, void *arg2);
extern struct qthread_mutex_impl *mutex_impl;
static unsigned long get_usecs(void);
void schedule(void*);
/* this is your qthread structure.
 */
struct qthread {
    void* stack;
    qthread_t next;
    void* stack_init;
    void* join_status;
    int exited;
    qthread_t join_thread;
    unsigned long wake_time;
    int io_fd;
    int io_type;   // 0 : read, 1 : write
    
};    

/* You'll probably want to define a thread queue structure, and
 * functions to append and remove threads. (Note that you only need to
 * remove the oldest item from the head, makes removal a lot easier)
 */
struct threadq {
    qthread_t head;
    qthread_t tail;
};

struct threadq active = {.head = NULL, .tail = NULL};
struct threadq sleep_q = {.head = NULL, .tail = NULL};
struct threadq io_q = {.head = NULL, .tail = NULL};

void* main_thread = NULL;
qthread_t cur_thread = NULL;
unsigned long earliest_wake;

void append_to_queue(struct threadq *queue, qthread_t thread){
	
    if (queue->head == NULL){
        queue->head = thread;
        queue->tail = thread;
        thread->next = NULL;
    }else{
        queue->tail->next = thread;
        queue->tail = thread;
        thread->next = NULL;
    }
    

}

int q_empty(struct threadq *queue){
	if (queue->head == NULL){
		return 1;
	}
	return 0;
}

qthread_t remove_from_queue(struct threadq *queue){
    if (queue->head == NULL){
        return NULL;
    }else{
        qthread_t ret = queue->head;
        queue->head = queue->head->next;
        ret->next = NULL;
        return ret;
    }
}

    
/* These are your actual mutex and cond structures - you'll allocate
 * them in qthread_mutex_init / cond_init and free them in  the
 * corresponding _destroy functions.
 */
struct qthread_mutex_impl {
    int locked;
    struct threadq *waitlist;
};

struct qthread_cond_impl {
    struct threadq *cond_q;
};


/* qthread_start, qthread_create - see lecture 7 00:01:00 to 00:06:00 for
 * a description of the difference. qthread_start takes a function
 * with two parameters; that function *must* call qthread_exit instead
 * of returning. qthread_create works the same way as pthread_create -
 * the function is allowed to return, so it needs a wrapper function.
 */
qthread_t qthread_start(void (*f)(void*, void*), void *arg1, void *arg2){
    qthread_t thread;
    thread = (qthread_t)calloc(sizeof(struct qthread), 1);
    //memset(thread, 0, sizeof(struct qthread));
    
    thread->exited = 0;
    
    thread->stack = calloc(8192, 1);
    thread->stack_init = thread->stack; // needed to free in exit
	//memset(thread->stack, 0, 8192);
	int i=0;
	for(i=0;i<8192/sizeof(void*);++i){
		thread->stack++;
	} 
    thread->stack = setup_stack(thread->stack, f, arg1, arg2);
    
    
    
    append_to_queue(&active,thread);
    return thread;
}

void wrapper(void* func, void *arg1){
	void* (*f)(void*) = (void* (*)(void*))func;
	void* ret = (*f)(arg1);
	qthread_exit(ret);
}

qthread_t qthread_create(void* (*f)(void*), void *arg1){
	
    return qthread_start(wrapper, f, arg1);
}

/* I suggest factoring your code so that you have a 'schedule'
 * function which selects the next thread to run and  switches to it,
 * or goes to sleep if there aren't any threads left to run.
 *
 * NOTE - if you end up switching back to the same thread, do *NOT*
 * use do_switch - just return from schedule(), since the saved stack
 * pointer for the current thread is old and invalid.
 */
 
 void remove_from_sleep_queue(qthread_t th){
	 qthread_t temp = sleep_q.head;
	 
	 if (temp == th){
		 sleep_q.head = sleep_q.head->next;
		 temp->next = NULL;
		 temp->wake_time = 0;
		 return;
	 }
	 qthread_t prev = temp;
	 qthread_t cur = temp->next;
	 
	 while(cur!=NULL){
		 if (cur==th){
			 prev->next = cur->next;
			 cur->next = NULL;
			 cur->wake_time = 0;
			 return;
		 }
		 cur = cur->next;
		 prev = prev->next;
	 }
	 
	 
 }
 fd_set rfds, wfds;
 void wakeup_thread(){
	 
	//usleep(earliest_wake - cur_time);
	int max = 0;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	qthread_t temp = io_q.head;
	while(temp!=NULL){
		if(temp->io_type == 0)
			FD_SET(temp->io_fd, &rfds);
		else if(temp->io_type == 1)
			FD_SET(temp->io_fd, &wfds);
			
		if (temp->io_fd > max){
				max = temp->io_fd;
		}
		temp = temp->next;
	}
	struct timeval tv;
	int select_ret = 0;
	tv.tv_sec = 0;

	unsigned long cur_time = get_usecs();

	if(cur_time > earliest_wake){
		tv.tv_usec = 0;
	}else{
		unsigned long utime = earliest_wake - cur_time;
		tv.tv_usec = utime;
	}
	
	select_ret = select(max+1, &rfds, &wfds, NULL, &tv);

	if (select_ret != 0){
		qthread_t t;
		while((t = remove_from_queue(&io_q)) != NULL){
			append_to_queue(&active, t);
		}
	}
	
	
	 
	 
	 qthread_t ptr = sleep_q.head;
	 int should_loop = 1;
	 cur_time = get_usecs();
	 if (cur_time < earliest_wake){
		 should_loop = 0;
		 // ideally the while loop below should come inside an if loop.
		 // but that would mean fixing the indentation which is too much work.
		 
	 }
	 
	 earliest_wake = 0;
	 while(should_loop && ptr != NULL){
		 // earliest_wake is invalid because the corresponding thread
		 // will be removed in this loop. So we need to update it.
		 if (ptr->wake_time > cur_time){
			 if ((earliest_wake == 0) || (earliest_wake > ptr->wake_time)){
				 earliest_wake = ptr->wake_time;
			 } 
			 ptr = ptr->next;
		 }else{
			 qthread_t th = ptr;
			 ptr = ptr->next;
			 remove_from_sleep_queue(th);
			 append_to_queue(&active, th);
		 }
		 
	 }
	 schedule(&cur_thread->stack);
 }
void schedule(void *save_location){

	qthread_t next_t = remove_from_queue(&active);
	if (next_t == cur_thread){
		return;
	}
	if (next_t == NULL){
		if (q_empty(&sleep_q) && q_empty(&io_q)){
			switch_to(NULL, main_thread);
		}else{
			wakeup_thread();
		}
	}else{
		cur_thread = next_t;
		switch_to(save_location, cur_thread->stack);
		
	}
}

/* qthread_run - run until the last thread exits
 */
void qthread_run(void){
	main_thread = calloc(sizeof(*main_thread), 1);
	//memset(main_thread, 0, sizeof(long));
	schedule(&main_thread);
	//free(main_thread);
}

/* qthread_yield - yield to the next runnable thread.
 */
void qthread_yield(void){
	//void* s_ptr = cur_thread->stack;
	append_to_queue(&active, cur_thread);
	schedule(&cur_thread->stack);	
}

/* qthread_exit, qthread_join - exit argument is returned by
 * qthread_join. Note that join blocks if the thread hasn't exited
 * yet, and is allowed to crash  if the thread doesn't exist.
 */
void qthread_exit(void *val){
	cur_thread->join_status = val;
	cur_thread->exited = 1;
	qthread_t th = cur_thread->join_thread;
	
	if(th != NULL){
		append_to_queue(&active, th);		
	}
	//free(cur_thread->stack_init);
	schedule(NULL);
	
}

void *qthread_join(qthread_t thread){
	
	void* ret;
    if(thread->exited == 1){
		ret = thread->join_status;
		free(thread->stack_init);
		free(thread);
		return ret;
	}
	thread->join_thread = cur_thread;
	schedule(&cur_thread->stack);
	ret = thread->join_status;
	free(thread->stack_init);
	free(thread);
	return ret;
	
}

/* Mutex functions
 */
void qthread_mutex_init(qthread_mutex_t *mutex){
	if (mutex == NULL){
		printf("mutex object is null. Initialization failed\n");
		return;
	}
	mutex->impl = (struct qthread_mutex_impl *)calloc(sizeof(struct qthread_mutex_impl), 1);
	mutex->impl->locked = 0;
	mutex->impl->waitlist = calloc(sizeof(struct threadq), 1);
	mutex->impl->waitlist->head = NULL;
	mutex->impl->waitlist->tail = NULL;
}
void qthread_mutex_destroy(qthread_mutex_t *mutex){
	if (mutex!=NULL){
		free(mutex->impl->waitlist);
		free(mutex->impl);
		
	}
}
void qthread_mutex_lock(qthread_mutex_t *mutex){
	if (!mutex->impl->locked){
		mutex->impl->locked = 1;
	}
	else{
		append_to_queue(mutex->impl->waitlist, cur_thread);
		schedule(&cur_thread->stack);
	}
}
void qthread_mutex_unlock(qthread_mutex_t *mutex){
	if (q_empty(mutex->impl->waitlist)){
		mutex->impl->locked = 0;
	}
	else{
		qthread_t next_t = remove_from_queue(mutex->impl->waitlist);
		append_to_queue(&active, next_t);
	}
}


/* Condition variable functions
 */
void qthread_cond_init(qthread_cond_t *cond){
	if (cond == NULL){
		printf("Condition variable is null. Initialization failed\n");
		return; 
	}
	cond->impl = (struct qthread_cond_impl *)calloc(sizeof(struct qthread_cond_impl), 1);
	cond->impl->cond_q = (struct threadq *)calloc(sizeof(struct threadq), 1);
	cond->impl->cond_q->head = NULL;
	cond->impl->cond_q->tail = NULL;
	
}
void qthread_cond_destroy(qthread_cond_t *cond){
	free(cond->impl->cond_q);
	free(cond->impl);
	
}
void qthread_cond_wait(qthread_cond_t *_cond, qthread_mutex_t *_mutex){
	append_to_queue(_cond->impl->cond_q, cur_thread);
	qthread_mutex_unlock(_mutex);
	schedule(&cur_thread->stack);
	qthread_mutex_lock(_mutex);
}
void qthread_cond_signal(qthread_cond_t *_cond){
	qthread_t th = remove_from_queue(_cond->impl->cond_q);
	if (th!=NULL){
		append_to_queue(&active, th);
	}
}
void qthread_cond_broadcast(qthread_cond_t *_cond){
	qthread_t th;
	while( (th = remove_from_queue(_cond->impl->cond_q)) != NULL){
		append_to_queue(&active, th);
	}
}

/* Helper function for the POSIX replacement API - you'll need to tell
 * time in order to implement qthread_usleep. (warning - store the
 * return value in 'unsigned long', not 'unsigned')
 */
static unsigned long get_usecs(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec*1000000 + tv.tv_usec;
}


/* POSIX replacement API. These are all the functions (well, the ones
 * used by the sample application) that might block.
 *
 * If there are no runnable threads, your scheduler needs to block
 * waiting for one of these blocking functions to return. You should
 * probably do this using the select() system call, indicating all the
 * file descriptors that threads are blocked on, and with a timeout
 * for the earliest thread waiting in qthread_usleep()
 */

/* qthread_usleep - yield to next runnable thread, making arrangements
 * to be put back on the active list after 'usecs' timeout. 
 */
void qthread_usleep(long int usecs){
	unsigned long wake_time = get_usecs() + usecs;
	if ((earliest_wake == 0) || (earliest_wake > wake_time)){
		earliest_wake = wake_time;
	}
	cur_thread->wake_time = wake_time;
	append_to_queue(&sleep_q, cur_thread);
	schedule(&cur_thread->stack);
}

/* qthread_read - see 'man 2 read' for API description.
 * make sure that the file descriptor is in non-blocking mode, try to
 * read from it, if you get -1 / EAGAIN then add it to the list of
 * file descriptors to go in the big scheduling 'select()' and switch
 * to another thread.
 */
ssize_t qthread_read(int fd, void *buf, size_t len)
{
    /* set non-blocking mode every time. If we added wrappers to
     * open(), socket(), etc. we could set non-blocking mode from the 
     * beginning, but this is a lot simpler (if less efficient). Do
     * this for _write and _accept, too.
     */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	while(1){
		int val = read(fd, buf, len);
		if(val != -1 || errno != EAGAIN){
			return val;
		}
		append_to_queue(&io_q,cur_thread);
		cur_thread->io_fd = fd;
		cur_thread->io_type = 0; // 0 : read
		schedule(&cur_thread->stack);
	}	
	return -1;
}

/* like read - make sure the descriptor is in non-blocking mode, check
 * if if there's anything there - if so, return it, otherwise save fd
 * and switch to another thread. Note that accept() counts as a 'read'
 * for the select call.
 */
int qthread_accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    while(1){
		int val = accept(fd, addr, addrlen);
		if(val != -1 || errno != EAGAIN){
			return val;
		}
		append_to_queue(&io_q,cur_thread);
		cur_thread->io_fd = fd;
		cur_thread->io_type = 0; // 0 : read
		schedule(&cur_thread->stack);
	}
	return -1;
}

/* Like read, except that it's an output, rather than an
 * input, so the file descriptor goes in the 'write flags' argument to
 * 'select'. It can block if the network is slow, although it's not
 * likely to in most of our testing.
 */
ssize_t qthread_write(int fd, void *buf, size_t len)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	while(1){
		int val = write(fd, buf, len);
		if(val != -1 || errno != EAGAIN){
			return val;
		}
		append_to_queue(&io_q,cur_thread);
		cur_thread->io_fd = fd;
		cur_thread->io_type = 1; // 1 : write
		schedule(&cur_thread->stack);
	}	
	return -1;
}

