/*
 * Copyright (c) 2001, 2002, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Driver code is in kern/tests/synchprobs.c We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

struct lock *male_lock;        /* Control access to male whales */
struct lock *female_lock;      /* Control access to female whales */
struct lock *matchmaker_lock;  /* Control access to matchmaker whales */
struct cv *male_cv;            /* Channel for male whales to sleep */
struct cv *female_cv;          /* Channel for female whales to sleep */
struct cv *matchmaker_cv;      /* Channel for matchmaker whales to sleep */

/*
 * Called by the driver during initialization.
 */

void whalemating_init() {
	male_lock = lock_create("male_lock");
	female_lock = lock_create("female_lock");
	matchmaker_lock = lock_create("matchmaker_lock");

	male_cv = cv_create("male_cv");
	female_cv = cv_create("female_cv");
	matchmaker_cv = cv_create("matchmaker_cv");
}

/*
 * Called by the driver during teardown.
 */

void
whalemating_cleanup() {
	cv_destroy(male_cv);
	cv_destroy(female_cv);
	cv_destroy(matchmaker_cv);

	lock_destroy(male_lock);
	lock_destroy(female_lock);
	lock_destroy(matchmaker_lock);
}

void
male(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling male_start and male_end when
	 * appropriate.
	 */
	male_start(index);

	lock_acquire(male_lock);
	cv_wait(male_cv, male_lock);  /* Wait to be woken up matchmaker */
	lock_release(male_lock);

	male_end(index);

	return;
}

void
female(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling female_start and female_end when
	 * appropriate.
	 */
	female_start(index);
	
	lock_acquire(female_lock);
	cv_wait(female_cv, female_lock);  /* Wait to be woken up matchmaker */
	lock_release(female_lock);

	female_end(index);

	return;
}

void
matchmaker(uint32_t index)
{
	(void)index;
	/*
	 * Implement this function by calling matchmaker_start and matchmaker_end
	 * when appropriate.
	 */
	matchmaker_start(index);

	lock_acquire(matchmaker_lock);
	lock_acquire(male_lock);
	lock_acquire(female_lock);

	cv_signal(male_cv, male_lock);      /* Get a male whale */
	cv_signal(female_cv, female_lock);  /* Get a female whale */

	lock_release(female_lock);
	lock_release(male_lock);
	lock_release(matchmaker_lock);

	matchmaker_end(index);

	return;
}
