/*
 *  pthreads.c
 *  MT2
 *
 *  Created by Ben on 27/08/11.
 *  Copyright 2011 Ben Torkington. All rights reserved.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sf2const.h"
#include "pthreads.h"
#include "task.h"

extern struct executive_t Exec;

#define NTHREADS MAX_TASKS

#ifdef __EMSCRIPTEN__

/*
 * Emscripten fiber-based cooperative multitasking.
 * Replaces pthreads with single-threaded coroutines via Asyncify stack switching.
 * This matches how the original 68000 arcade hardware worked.
 */

#include <emscripten/fiber.h>
#include <setjmp.h>

#define FIBER_STACK_SIZE  65536
#define ASYNCIFY_BUF_SIZE 8192

static emscripten_fiber_t main_fiber;
static emscripten_fiber_t task_fibers[NTHREADS];
static char fiber_c_stacks[NTHREADS][FIBER_STACK_SIZE];
static char fiber_asyncify_stacks[NTHREADS][ASYNCIFY_BUF_SIZE];
static char main_asyncify_stack[ASYNCIFY_BUF_SIZE];
static int fiber_alive[NTHREADS];
static int fiber_kill_flag[NTHREADS];

static void fiber_entry(void *arg) {
    Task *task = (Task *)arg;
    if (task->code) {
        task->code();
    }
    {
        static int fiber_died_count = 0;
        if (fiber_died_count++ < 5)
            printf("FIBER DIED: task %d code returned!\n", task->RHThreadID);
    }
    task->status = TASK_EMPTY;
    fiber_alive[task->RHThreadID] = 0;
    emscripten_fiber_swap(&task_fibers[task->RHThreadID], &main_fiber);
}

void RHWait(Task *task) {
    /* Yield back to the dispatcher */
    emscripten_fiber_swap(&task_fibers[task->RHThreadID], &main_fiber);

    /* Check kill flag after being resumed */
    if (fiber_kill_flag[task->RHThreadID]) {
        fiber_kill_flag[task->RHThreadID] = 0;
        task->status = TASK_EMPTY;
        fiber_alive[task->RHThreadID] = 0;
        emscripten_fiber_swap(&task_fibers[task->RHThreadID], &main_fiber);
    }
}

void RHCleanup(Task *task) {
    /* no-op for fibers */
}

void RHExit(Task *task) {
    task->status = TASK_EMPTY;
    fiber_alive[task->RHThreadID] = 0;
    emscripten_fiber_swap(&task_fibers[task->RHThreadID], &main_fiber);
}

void RHKill(Task *task) {
    fiber_kill_flag[task->RHThreadID] = 1;
}

void RHResume(Task *task) {
    if (!fiber_alive[task->RHThreadID]) return;
    /* Swap to the task fiber; it swaps back when it yields */
    emscripten_fiber_swap(&main_fiber, &task_fibers[task->RHThreadID]);
}

void RHCreateThread(int worker) {
    emscripten_fiber_init(
        &task_fibers[worker],
        fiber_entry,
        &Exec.Tasks[worker],
        fiber_c_stacks[worker],
        FIBER_STACK_SIZE,
        fiber_asyncify_stacks[worker],
        ASYNCIFY_BUF_SIZE
    );
    fiber_alive[worker] = 1;
    fiber_kill_flag[worker] = 0;
}

void RHInitThreads(void) {
    printf("RedHammer: fibers init\n");
    emscripten_fiber_init_from_current_context(
        &main_fiber,
        main_asyncify_stack,
        ASYNCIFY_BUF_SIZE
    );
    for (int i = 0; i < NTHREADS; i++) {
        Exec.Tasks[i].RHThreadID = i;
        fiber_alive[i] = 0;
        fiber_kill_flag[i] = 0;
    }
    Exec.FreeTasks = 8;
}

#else /* native pthreads */

#include <pthread.h>
#include <unistd.h>

void *RHThreadWorker(void *arg);

#define errexit(code, str)							\
  fprintf(stderr, "%s: %s\n",(str),strerror(code));	\
  exit(1);

pthread_t	    pt_threads[NTHREADS];
pthread_mutex_t ptmx_go_task;
pthread_cond_t  ptcv_go_task;

pthread_mutex_t	ptmx_despatcher;
pthread_cond_t	ptcv_despatcher;

int InChild;

char pt_go_task[NTHREADS];


// RHWait(task): yield execution after setting time to sleep

void RHWait(Task *task) {
	int state;
	//printf("RHWait: worker %d waits (%s)\n", task->RHThreadID, task->signal);
	pthread_mutex_lock(&ptmx_despatcher);
	InChild = FALSE;
	pthread_mutex_unlock(&ptmx_despatcher);

	pthread_mutex_lock(&ptmx_go_task);
	pthread_cond_broadcast(&ptcv_despatcher);

	pt_go_task[task->RHThreadID] = FALSE;

	while (!pt_go_task[task->RHThreadID]) {
		pthread_cond_wait(&ptcv_go_task, &ptmx_go_task);
	}
	state = pt_go_task[task->RHThreadID];
	if (state == -1) {
		pt_go_task[task->RHThreadID] = 0;
	}
	pthread_mutex_unlock(&ptmx_go_task);
	if (state == -1) {
		printf("killed\n");

		pthread_exit(NULL);
	}
	//printf("RHWait: worker %d awakens\n", task->RHThreadID);
}
void RHCleanup(Task *task) {
	pthread_mutex_unlock(&ptmx_go_task);
}
void RHExit(Task *task) {
	//printf("RHExit worker\n");
	pthread_mutex_lock(&ptmx_go_task);
	pt_go_task[task->RHThreadID] = FALSE;
	pthread_mutex_unlock(&ptmx_go_task);

	pthread_mutex_lock(&ptmx_despatcher);
	InChild = FALSE;
	pthread_cond_broadcast(&ptcv_despatcher);
	pthread_mutex_unlock(&ptmx_despatcher);
	//printf("Exiting Thread\n");
	pthread_exit(NULL);
}
void RHKill (Task *task) {

//	if(pthread_cancel(pt_threads[task->RHThreadID])) {
//		printf("error cancelling task %x",task);
//	}
	pthread_mutex_lock(&ptmx_go_task);
	pt_go_task[task->RHThreadID] = -1;
	pthread_cond_broadcast(&ptcv_go_task);
	pthread_mutex_unlock(&ptmx_go_task);

}
void RHResume(Task *task) {
	pthread_mutex_lock(&ptmx_despatcher);
	InChild = TRUE;

	pthread_mutex_lock(&ptmx_go_task);
	pt_go_task[task->RHThreadID] = TRUE;
	pthread_cond_broadcast(&ptcv_go_task);
	pthread_mutex_unlock(&ptmx_go_task);

	// now wait until the task yeilds
	while (InChild) {
		pthread_cond_wait(&ptcv_despatcher, &ptmx_despatcher);
	}
	pthread_mutex_unlock(&ptmx_despatcher);
	if (task->status == TASK_EMPTY) {
		//printf("pthread_join()\n");
		pthread_join(pt_threads[task->RHThreadID], NULL);
	}
	//printf("Back in despatcher\n");
}

void RHCreateThread(int worker) {
	int errcode;
	if ((errcode = pthread_create(&pt_threads[worker], NULL, RHThreadWorker, &Exec.Tasks[worker]))) {
		errexit(errcode, "pthread_create");
	}
	//printf("RHCreateThread: worker %d\n", worker);
}

void RHInitThreads(void) {
	int i;
	printf("RedHammer: threads init\n");
	for (i=0; i<NTHREADS; ++i) {
		Exec.Tasks[i].RHThreadID = i;
	}
	pthread_mutex_init(&ptmx_go_task, NULL);
	pthread_cond_init (&ptcv_go_task, NULL);
	pthread_mutex_init(&ptmx_despatcher, NULL);
	pthread_cond_init(&ptcv_despatcher, NULL);

	Exec.FreeTasks = 8;
}
void *RHThreadWorker(void *arg) {
	Task *task = (Task *)arg;
	// wait until our turn, then begin
	pthread_mutex_lock(&ptmx_go_task);
	while (!pt_go_task[task->RHThreadID]) {
		pthread_cond_wait(&ptcv_go_task, &ptmx_go_task);
	}
	pthread_mutex_unlock(&ptmx_go_task);

	//printf("RHThreadWorker worker %d %s beginning\n", task->RHThreadID, task->name);
	if(task->code) {
		task->code();
	} else {
		printf("NULL task %d\n", task->RHThreadID);
	}
	// task should have called TASK_DIE by now
	// in case it hasn't
	task->status = TASK_EMPTY;
	pthread_mutex_lock(&ptmx_go_task);
	pt_go_task[task->RHThreadID] = FALSE;
	pthread_mutex_unlock(&ptmx_go_task);

	//printf("RHThreadWorker worker %d ends\n", task->RHThreadID);
	pthread_mutex_lock(&ptmx_despatcher);
	InChild = FALSE;
	pthread_cond_broadcast(&ptcv_despatcher);
	pthread_mutex_unlock(&ptmx_despatcher);
	pthread_exit(NULL);
	// return pointer to exit status if necessary
	return NULL;
}

#endif
