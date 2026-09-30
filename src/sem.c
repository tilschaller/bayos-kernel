#include <sem.h>
#include <sched.h>
#include <io.h>

//
// we need some lock, which is dependent on the scheduler
// i am gonna be implementing semaphores
//
semaphore *sem_create(semaphore *sem, int initial_count)
{
	sem->count = initial_count;
	sem->wait_head = NULL;
	sem->wait_tail = NULL;

	return sem;
}

void sem_wait(semaphore *sem)
{
	unsigned long flags = save_irqdisable();
	sem->count--;

	if (sem->count < 0) {
		get_current_process()->status = BLOCKED;
		get_current_process()->wait_next = NULL;

		if (sem->wait_tail) {
			sem->wait_tail->wait_next = get_current_process();
		}
		else {
			sem->wait_head = get_current_process();
		}
		sem->wait_tail = get_current_process();
		irqrestore(flags);
		yield();
		return;
	}

	irqrestore(flags);
}


// returns 1 if acquired without blocking, 0 if would have blocked
int sem_trywait(semaphore *sem)
{
	unsigned long flags = save_irqdisable();

	if (sem->count > 0) {
		sem->count--;
		irqrestore(flags);
		return 1;
	}

	irqrestore(flags);
	return 0;
}

void sem_signal(semaphore *sem)
{
	unsigned long flags = save_irqdisable();

	sem->count++;

	if (sem->count <= 0 && sem->wait_head != NULL) {
		process *woken = sem->wait_head;
		sem->wait_head = woken->wait_next;
		if (sem->wait_head == NULL) sem->wait_tail = NULL;
		woken->wait_next = NULL;
		woken->status = READY;
	}

	irqrestore(flags);
}
