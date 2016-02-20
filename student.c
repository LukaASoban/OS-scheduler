/*
 * student.c
 * Multithreaded OS Simulation Project
 * Fall 2015
 *
 * This file contains the CPU scheduler for the simulation.
 * Name: Luka Antolic-Soban
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os-sim.h"


extern void insert_to_queue(pcb_t *process);
extern int rankPCB(pcb_t *process1, pcb_t *process2);
extern pcb_t* remove_from_queue();


/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;
static int cpu_count;

pthread_mutex_t lock_for_queue; /* ready queue mutex lock*/
pthread_cond_t stop_idle; /* condition variable for queue not being empty and to stop idle state*/

int time_slice; /* -1 for FIFO and SP (this is the time quantum)*/
static int algorithm; /*FIFO = 0, RR = 1, SP = 2*/


/*the actual ready queue itself*/
static pcb_t* front;
static int length;
/*******************************/



/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running
 *	process indexed by the cpu id. See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{

	pcb_t* process_runable = NULL;


    /* if remove_from_queue returned a process (if there was one in the queue) */
    if(length > 0) {
    	pthread_mutex_lock(&current_mutex); //LOCKED ----------------------------

		/*Select and remove a runnable process from the ready queue*/
    	process_runable = remove_from_queue();
    	
    	process_runable->state = PROCESS_RUNNING; //Set the process state to RUNNING
    	current[cpu_id] = process_runable;

    	pthread_mutex_unlock(&current_mutex); //UNLOCKED ------------------------
    }

    /*remove_from_queue must have returned  NULL so we switch to an idle process*/
    context_switch(cpu_id, process_runable, time_slice);

}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{
   pthread_mutex_lock(&lock_for_queue); //LOCKED ---------------------------------

   while(front == NULL) {
   		pthread_cond_wait(&stop_idle, &lock_for_queue);
   }

   pthread_mutex_unlock(&lock_for_queue);

   schedule(cpu_id);
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{

	pcb_t* running_process;

    /*put the currently running process back in the ready queue*/
    pthread_mutex_lock(&current_mutex); //LOCKED --------------------------------

    running_process = current[cpu_id];

    if(running_process != NULL) {
    	running_process->state = PROCESS_READY;
    	insert_to_queue(running_process);
    }

    pthread_mutex_unlock(&current_mutex); //UNLOCKED ----------------------------

    /*NOW we scheudle a new runnable process*/
    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{

	pcb_t* running_process;

    pthread_mutex_lock(&current_mutex); //LOCKED --------------------------------

    running_process = current[cpu_id];

    if(running_process != NULL)
    {
    	running_process->state = PROCESS_WAITING;
    }

    pthread_mutex_unlock(&current_mutex); //UNLOCKED ----------------------------

    /*NOW we scheudle a new runnable process*/
    schedule(cpu_id);
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{

	pcb_t* running_process;
	pthread_mutex_lock(&current_mutex); //LOCKED --------------------------------
	running_process = current[cpu_id];
    /* We will make the process terminated*/
    if(running_process != NULL)
    {
    	running_process->state = PROCESS_TERMINATED;
    }
    pthread_mutex_unlock(&current_mutex); //UNLOCKED ----------------------------

    /*NOW we scheudle a new runnable process*/
    schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is static priority, wake_up() may need
 *      to preempt the CPU with the lowest priority process to allow it to
 *      execute the process which just woke up.  However, if any CPU is
 *      currently running idle, or all of the CPUs are running processes
 *      with a higher priority than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
    int lowest_priority = 11; 
    int lowest_priority_cpuID = -1;
    
    int i = 0;

    process->state = PROCESS_READY;
    insert_to_queue(process);
    	

    	/* if the scheulder is SP then we need to preempt CPU*/
    	if(algorithm == 2) {
    		pthread_mutex_lock(&current_mutex); //LOCKED -------------------------

    		/*loop through running procs*/
    		while(i < cpu_count) {

    			pcb_t* runningPROC = current[i];

    			if(runningPROC != NULL) {
    				//if the CPU contains process of lower priority we makerr it
    				if(runningPROC->static_priority <= lowest_priority)
    				{
    					lowest_priority_cpuID = i;
    					lowest_priority = runningPROC->static_priority;
    				}

    			} else {
    				/* if process is idle or terminated we then return*/
    				pthread_mutex_unlock(&current_mutex); //UNLOCKED --------------
    				return;
    			}

    			i++;
    		}

    		pthread_mutex_unlock(&current_mutex); //UNLOCKED -----------------------
    		/*we now force_preempt since we did not find idle or terminated processes*/
    		if(lowest_priority_cpuID >= 0) { force_preempt(lowest_priority_cpuID); }

    	}



}


/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -r and -p command-line parameters.
 */
int main(int argc, char *argv[])
{
    

    /* Parse command-line arguments */
    if (argc < 2)
    {
        fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -p : Static Priority Scheduler\n\n");
        return -1;
    }
    cpu_count = atoi(argv[1]);

    
    if(argc == 4 && (argv[2][1] == 'r')) {
    	algorithm = 1;
    	time_slice = atoi(argv[3]);
    } else if(argc == 3 && (argv[2][1] == 'p')) {
    	algorithm = 2;
    	time_slice = -1;
    } else if(argc != 2) {
    	fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p ]\n"
            "    Default : FIFO Scheduler\n"
            "         -r : Round-Robin Scheduler\n"
            "         -p : Static Priority Scheduler\n\n");
        return -1;
    } else {
    	algorithm = 0;
    	time_slice = -1;
    }


    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);

    pthread_cond_init(&stop_idle, NULL);
    pthread_mutex_init(&lock_for_queue, NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);
    free(current);

    return 0;
}




/*comparison function for the PCBs insides of the quueue */
/* PARAM: two processes to be compared to their current scheduling type*/
/* RETURN: -1 if process 1 comes before process 2 othereise we return 1*/
extern int rankPCB(pcb_t *proc1, pcb_t* proc2) {

	int priority1= proc1->static_priority;
	int priority2= proc2->static_priority;
	int came_before;
	switch (algorithm)
	{
		case 0:
			return 1;
		case 1:
			return 1;
		default:
			came_before = priority2 - priority1;
		 	//if processes are same priority then we make it a FIFO

			if(came_before == 0) { return 1; }

			return came_before;
	}

	
}

/*This function is ued to add a process control block to the ready queue*/
/* PARAM: It takes in a pointer to the PCB that is going to be added*/
extern void insert_to_queue(pcb_t *process) {

	pthread_mutex_lock(&lock_for_queue); //LOCKED -------------------------------------

	pcb_t *inuse = front;
	pcb_t *past = NULL;

	/*We want to loop until we find place to add*/
	while(inuse != NULL && (rankPCB(process, inuse) > 0)) {
		past = inuse;
		inuse = inuse->next;
	}

	//we aren't at the front so we can insert as normal
	
	if(past != NULL) {
		process->next = inuse;
		past->next = process;
	} else {
		/* If we are are the front of the queue then we update it*/
		

		process->next = inuse;
		front = process;
	}

	/*increment the length by 1 since we inserted something into the queue*/
	length = (length + 1);


	// process->next = NULL;
	// if(front == NULL) {
	// 	front = process;
	// 	back = process;
	// }
	// else {
		
	// 	if(algorithm == 0) {
	// 		insert_FIFO(process);
	// 	}
	// 	else if(algorithm == 1) {
	// 		insert_RR(process);
	// 	}
	// 	else if(algorithm == 2) {
	// 		insert_SP(process);
	// 	}
	// }

	pthread_mutex_unlock(&lock_for_queue); //UNLOCKED ------------------------------------
	pthread_cond_signal(&stop_idle); //signal the thread to start while queue !empty



}

/* This function is used to delete a PCB from the ready queue*/
/* PARAM: NONE, Simply removes a PCB from the front of the queue
	RETURN: the PCB that was taken from the front*/
extern pcb_t* remove_from_queue() {

	pcb_t* block;
	pthread_mutex_lock(&lock_for_queue); //LOCKED -----------------------------------------

	block = front;

	if(front == NULL)
	{
		return block;
	}

	
	/* If there is not something on top of the queue then go to the next PCB */
		front = front->next;
		length = length - 1;

	pthread_mutex_unlock(&lock_for_queue); //UNLOCKED -------------------------------------

	return block;
}
