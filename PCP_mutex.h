/********************************************************************************
  * @file    PCP_mutex.h
  * @author  Mohammed Al-Sanabani
	// NOTE: Explanation for the calculations are inline
  * @version V1.0.0
  * @date    5-april-2014
  * @brief   Main program body
  ******************************************************************************/

#ifndef PCP_MUTEX_H
#define PCP_MUTEX_H
	

#include "cpu_utils.h"
#include "task.h"
#include "Global.h"
#include "portmacro.h"


#define PCP_MutexID  U16


#define task0XPos    20
#define task1XPos    50
#define task2XPos    80
#define task3XPos    110
#define task4XPos    140
#define idleXPos     170


#ifndef bool
    #define bool int
    #define false ((bool)0)
    #define true  ((bool)1)
#endif
	
 // blocked processes linked list.
typedef struct BlockedProcesses
  {
		struct BlockedProcesses    *      next;
		xTaskHandle                       process;
  }BlockedProcesses;
			
// Semaphore Control Block
typedef struct PCP_Mutex
	{
		struct PCP_Mutex *                next;
		PCP_MutexID                       id;
		bool                              locked;
		unsigned portBASE_TYPE            priorityCeling;
		xTaskHandle                       taskHoldingPCP_Mutex;
		signed char       *               nameTaskHoldingPCP_Mutex;
		BlockedProcesses  *               rootQueueTasksBlocked;
	}PCP_Mutex;

	PCP_MutexID createPCP_Mutex(unsigned portBASE_TYPE priority);
  void PCP_MutexLock(PCP_Mutex * mutex);
	void PCP_MutexUnlock(PCP_Mutex * mutex);
	PCP_Mutex * findPCP_Mutex(PCP_MutexID mutexID);
  void drawInTask(xTaskHandle task, PCP_MutexID id, bool lock, bool success);
	void drawStartEndTask(xTaskHandle task);

	static  unsigned portBASE_TYPE systemCeiling(xTaskHandle currentTask,  xTaskHandle* taskCausingCeling);
	static	PCP_MutexID createPCP_mutexLocal(unsigned portBASE_TYPE);
	static  void insertTaskInBlockedQueue_ofLockedPCP_Mutexs(xTaskHandle task);
	static  void insertTaskInBlockedQueue_ofLockedPCP_Mutexs2(xTaskHandle task);
  static  bool taskInBlockedList(struct PCP_Mutex * mutex, xTaskHandle task);
  static  unsigned portBASE_TYPE maxPriority(unsigned portBASE_TYPE, unsigned portBASE_TYPE);



#endif
