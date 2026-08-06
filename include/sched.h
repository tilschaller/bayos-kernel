#ifndef _SCHED_H
#define _SCHED_H

#include <stdint.h>

struct allocator;
void sched_init(struct allocator *al);

void add_process(uintptr_t func);
void mark_current_proc_as_dead(void);

int sem_create(int initial_count);
void sem_wait(int sem_id);
int sem_trywait(int sem_id);
void sem_signal(int sem_id);
void sem_destroy(int sem_id);

#define mutex_create()		sem_create(1)
#define mutex_lock(m)		sem_wait(m)
#define mutex_try_lock(m)	sem_trywait(m)
#define mutex_unlock(m)		sem_signal(m)
#define mutex_destroy(m)	sem_destroy(m)

//
// these are the mutex function you are most likely to want to use
//
// these defines a mutex type
#define DEFINE_MUTEX_TYPE(T) \
	typedef struct { \
		T *data; \
		int lock; \
	} Mutex_##T

// this is how you would refer to a mutex type
#define Mutex(T) Mutex_##T

#define MUTEX_LOCK(m)	( mutex_lock((m).lock), (m).data )
#define MUTEX_TRY_LOCK(m) ( mutex_try_lock((m).lock) ? (m).data : NULL )
#define MUTEX_UNLOCK(m) mutex_unlock((m).lock)

int pipe_create(size_t buffer_len);
int pipe_write(int pipe_id, const uint8_t *data, size_t len);
int pipe_try_write(int pipe_id, uint8_t byte);
int pipe_read(int pipe_id, uint8_t *out, size_t len);
void pipe_close(int pipe_id);

typedef enum {
	EMPTY = 0,
	PIPE,
} resource_type;

typedef struct {
	resource_type type;
	int type_id;
} resource;

// add a resource to the process struct
// returns the file descriptor
int resource_add(resource_type type, int type_id);
// remove it from the process struct
// this returns the type_id
// you need to deallocate it yourself
// if no resources is there, returns -1
int resource_remove(int fd);

// read from a resource
int resource_read(int fd, uint8_t *buf, size_t len);
// write to a resource
int resource_write(int fd, const uint8_t *buf, size_t len);

#endif // _SCHED_H
