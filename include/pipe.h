#ifndef _PIPE_H
#define _PIPE_H

#include <stdint.h>
#include <sem.h>

typedef struct {
	uint8_t *buffer;
	size_t buffer_len;

	size_t read_pos;
	size_t write_pos;

	int read_refs;
	int write_refs;

	semaphore *buf_lock;
	semaphore *sem_empty;
	semaphore *sem_full;
} pipe;

pipe *pipe_create(size_t buffer_len);
int pipe_write(pipe *pipe_id, const uint8_t *data, size_t len);
int pipe_try_write(pipe *pipe_id, uint8_t byte);
int pipe_read(pipe *pipe_id, uint8_t *out, size_t len);
void pipe_close(pipe *pipe_id);

#endif // _PIPE_H
