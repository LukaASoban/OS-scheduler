/*
 * student.h
 * Multithreaded OS Simulation Project - Fall 2016
 *
 *
 */

#ifndef __STUDENT_H__
#define __STUDENT_H__

#include "os-sim.h"

/* Function declarations */
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);

#endif /* __STUDENT_H__ */
