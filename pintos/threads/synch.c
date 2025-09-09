/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

// sema_up: 락 해제 V
// sema_down: 락 획득 P
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;
	struct thread* current_thread = thread_current();

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_insert_ordered(&sema->waiters, &current_thread->elem, thread_priority_comp, NULL);
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	// struct thread* th = list_entry (list_pop_front(&sema->waiters), struct thread, elem);
	enum intr_level old_level;
	
	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
		thread_unblock(list_entry (list_pop_front(&sema->waiters), struct thread, elem));

	sema->value++;
	intr_set_level(old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */

/*
	LOCK을 획득합니다. 만약 락을 즉시 사용할 수 없다면, 사용 가능해질 때까지 대기(sleep) 상태에 들어갑니다.
	이 락은 현재 스레드가 이미 소유하고 있는 상태여서는 안 됩니다.
	이 함수는 대기 상태에 들어갈 수 있으므로, 인터럽트 핸들러 내에서 호출해서는 안 됩니다.
	함수가 호출될 때 인터럽트가 비활성화된 상태일 수는 있으나, 만약 스레드가 대기 상태로 전환되어야 한다면 인터럽트는 다시 활성화됩니다.
*/
// 스레드가 락을 획득하지 못해 잠들기 직전
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	enum intr_level old_level;
	
	// 1. 탐색을 시작할 임시 lock 포인터 변수를 만듭니다.
	struct lock* temp_lock = lock;
	struct thread* current_thread = thread_current();

	// 2. 더 이상 올라갈 곳이 없을 때까지 루프를 돕니다.
	while (temp_lock != NULL) {
		// 3. 현재 탐색 중인 락(temp_lock)의 소유자가 있는지, 그리고 기부가 필요한지 확인합니다.
		if (temp_lock->holder != NULL && current_thread->priority > temp_lock->holder->priority) {
			
			// 4. 현재 락의 소유자(temp_lock->holder)에게 우선순위를 기부합니다.
			temp_lock->holder->priority = current_thread->priority;
			
			// 5. 다음 탐색을 위해, 소유자가 기다리는 락으로 포인터를 옮깁니다.
			temp_lock = temp_lock->holder->waiting_on_lock; 
			
		} else {
			// 기부가 필요 없거나 소유자가 없으면 루프를 탈출합니다.
			break; 
		}
	}

	// 잠들기 직전, 내가 이 락을 기다린다고 기록합니다.
	current_thread->waiting_on_lock = lock;

	sema_down (&lock->semaphore);
	// 깨어난 후, 더 이상 기다리지 않으므로 기록을 지웁니다.
    current_thread->waiting_on_lock = NULL;

	old_level = intr_disable();
	lock->holder = current_thread;
	list_push_back(&current_thread->holding_locks, &lock->elem);
	intr_set_level(old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
// 내가 가진 책임(락) 중 하나를 내려놓고, 남아있는 다른 책임들을 고려하여 내 현재 중요도(우선순위)를 재조정한 뒤, 다음 사람에게 락을 넘겨주는 것
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));
	struct thread* lock_thread = lock->holder;
	list_remove(&lock->elem);
	/*
	1. 기준점 설정: 현재 스레드의 우선순위를 일단 자신의 **original_priority**로 되돌립니다. 이것이 돌아갈 수 있는 최저점입니다.
	2. 남아있는 책임들 확인: holding_locks 리스트에 아직 다른 락들이 남아있는지 확인하고, 리스트를 순회합니다.
	3. 최고 대기자 찾기: 각각의 남아있는 락(the_lock)에 대해, 그 락을 기다리는 스레드들의 대기열(the_lock->semaphore.waiters)에서 가장 우선순위가 높은 스레드를 찾습니다.
	4. 대기열이 이미 우선순위 순으로 정렬되어 있다면, list_front()로 맨 앞의 스레드만 확인하면 됩니다.
	5. 우선순위 갱신: 만약 3번에서 찾은 대기자의 우선순위가 현재 스레드의 우선순위보다 높다면, 현재 스레드의 우선순위를 그 높은 값으로 갱신합니다.
	*/
	struct thread* cur_thread = thread_current();
	cur_thread->priority = lock->holder->original_priority;
	int max_value = cur_thread->original_priority;

	if (!list_empty(&cur_thread->holding_locks)) {
		struct list_elem* head = list_head(&cur_thread->holding_locks);
		while (head != list_end(&cur_thread->holding_locks)) {
			struct lock *held_lock = list_entry(head, struct lock, elem);
			if (!list_empty(&held_lock->semaphore.waiters)) {
				struct thread *waiter = list_entry(list_front(&held_lock->semaphore.waiters), struct thread, elem);
				if (waiter->priority > max_value) {
					max_value = waiter->priority;
				}
			}
			head = list_next(head);
		}
	}

	cur_thread->priority = max_value;
	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/*
	조건 변수(condition variable) COND를 초기화합니다.
	조건 변수는 코드의 한 부분이 특정 조건이 충족되었음을 알리는 **신호(signal)**를 보내면, 
	이와 협력하는 다른 코드가 그 신호를 받아 특정 동작을 수행할 수 있게 해주는 도구입니다.
*/
/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
