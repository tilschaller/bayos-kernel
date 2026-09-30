#ifndef _SEM_H
#define _SEM_H

#include <sched.h>

typedef struct {
	int count;
	process *wait_head;
	process *wait_tail;
} semaphore;

semaphore *sem_create(semaphore *sem, int initial_count);
void sem_wait(semaphore *sem);
int sem_trywait(semaphore *sem);
void sem_signal(semaphore *sem);

typedef semaphore mutex;

#define mutex_create(m)		sem_create(m, 1)
#define mutex_lock(m)		sem_wait(m)
#define mutex_try_lock(m)	sem_trywait(m)
#define mutex_unlock(m)		sem_signal(m)

//
// these are the mutex function you are most likely to want to use
//
// these defines a mutex type
#define DEFINE_MUTEX_TYPE(T) \
	typedef struct { \
		T *data; \
		semaphore *lock; \
	} Mutex_##T

// this is how you would refer to a mutex type
#define Mutex(T) Mutex_##T

#define MUTEX_LOCK(m)	( mutex_lock((m).lock), (m).data )
#define MUTEX_TRY_LOCK(m) ( mutex_try_lock((m).lock) ? (m).data : NULL )
#define MUTEX_UNLOCK(m) mutex_unlock((m).lock)

#endif // _SEM_H
