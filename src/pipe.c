#include <pipe.h>
#include <sem.h>
#include <io.h>
#include <alloc.h>

pipe *pipe_create(size_t buffer_len)
{
	allocator *al = MUTEX_LOCK(g_al);
	pipe *p = alloc(al, sizeof(pipe));
	p->buffer = alloc(al, buffer_len);
	semaphore *s = alloc(al, sizeof(mutex) * 3);
	MUTEX_UNLOCK(g_al);

	p->buffer_len = buffer_len;
	p->read_pos = 0;
	p->write_pos = 0;
	p->read_refs = 1;
	p->write_refs = 1;
	p->buf_lock = mutex_create(s);
	p->sem_empty = sem_create(s + 1, buffer_len);
	p->sem_full = sem_create(s + 2, 0);

	return p;
}

int pipe_write(pipe *p, const uint8_t *data, size_t len)
{
	if (p->read_refs == 0)
		return -1;

	for (size_t i = 0; i < len; i++) {
		sem_wait(p->sem_empty);

		if (p->read_refs == 0) {
			return i;
		}

		mutex_lock(p->buf_lock);
		p->buffer[p->write_pos] = data[i];
		p->write_pos = (p->write_pos + 1) % p->buffer_len;
		mutex_unlock(p->buf_lock);

		sem_signal(p->sem_full);
	}

	return len;
}

int pipe_try_write(pipe *p, uint8_t byte)
{
	if (p->read_refs == 0)
		return -1;

	if (!sem_trywait(p->sem_empty)) {
		return 0;
	}

	unsigned long flags = save_irqdisable();

	// TODO: check if it is a problem
	// the buffer mutex lock is not acquired

	p->buffer[p->write_pos] = byte;
	p->write_pos = (p->write_pos + 1) % p->buffer_len;

	irqrestore(flags);

	sem_signal(p->sem_full);
	return 1;
}

int pipe_read(pipe *p, uint8_t *out, size_t len)
{
	size_t i = 0;

	if (len == 0) return 0;

	if (p->write_refs == 0 && p->sem_full->count == 0)
		return 0;

	sem_wait(p->sem_full);
	mutex_lock(p->buf_lock);
	out[i] = p->buffer[p->read_pos];
	p->read_pos = (p->read_pos + 1) % p->buffer_len;
	mutex_unlock(p->buf_lock);
	sem_signal(p->sem_empty);
	i++;

	for (; i < len; i++) {
		if (!sem_trywait(p->sem_full))
			break; // nothing more ready right now
		mutex_lock(p->buf_lock);
		out[i] = p->buffer[p->read_pos];
		p->read_pos = (p->read_pos + 1) % p->buffer_len;
		mutex_unlock(p->buf_lock);
		sem_signal(p->sem_empty);
	}

	return i;
}

void pipe_close(pipe *p)
{
	allocator *al = MUTEX_LOCK(g_al);

	free(al, p->buf_lock);
	free(al, p->buffer);
	free(al, p);

	MUTEX_UNLOCK(g_al);
}
