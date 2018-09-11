/********************************************************************************
  * @file    PCP_mutex.c
  * @author  Mohammed Al-Sanabani
	// NOTE: Explanation for the calculations are inline
  * @version V1.0.0
  * @date    5-april-2014
  ******************************************************************************/
	
#include "PCP_mutex.h"
#include "stdlib.h" 
#include "stdint.h"
#include "task.h"
#include "stm32f429i_discovery_lcd.h"
#include "stm32f429i_discovery_ioe.h"
#include "main.h"

// use void LCD_DrawChar(uint16_t Xpos, uint16_t Ypos, const uint16_t *c)
// to draw to the current line number when locking/unlocking mutex / resuming 
// process
extern unsigned int lineNo;
extern xTaskHandle t0Handle;
extern xTaskHandle t1Handle;
extern xTaskHandle t2Handle;
extern xTaskHandle t3Handle;
extern xTaskHandle t4Handle;

struct PCP_Mutex *                  rootOfPCP_MutexList    = NULL;
struct PCP_Mutex *                  endOfPCP_MutexList     = NULL;

PCP_MutexID createPCP_Mutex (unsigned portBASE_TYPE priority)
{
	PCP_MutexID mutexID = 0;
	
	taskENTER_CRITICAL();
	vTaskSuspendAll(); // might not be needed as context switches are disabled above
	
  mutexID = createPCP_mutexLocal(priority);
	
	taskEXIT_CRITICAL();
	xTaskResumeAll();
	
	return mutexID;
}

static PCP_MutexID createPCP_mutexLocal(unsigned portBASE_TYPE priority)
{
	// expect context switching to have been disabled before calling the function
	static PCP_MutexID mutexID = 0;
	
	struct PCP_Mutex * newElement          = (struct PCP_Mutex*)calloc(1, sizeof(PCP_Mutex));
	newElement->next                       = NULL;
	newElement->id                         = mutexID;
	newElement->locked                     = false;
	newElement->priorityCeling             = priority;
	newElement->taskHoldingPCP_Mutex       = NULL;
	newElement->nameTaskHoldingPCP_Mutex   = NULL;
	newElement->rootQueueTasksBlocked      = NULL;	
			
	if (0 == mutexID)
	{
	  // first mutex to be created
		rootOfPCP_MutexList = newElement;
	}
	else
	{
		// insert new element to list
		endOfPCP_MutexList->next = newElement;
	}
	
	endOfPCP_MutexList  = newElement;
	
	mutexID++;

	return newElement->id;
}

uint16_t getMutexColor(PCP_MutexID id)
{
	switch (id)
	{
		case 0:
			return LCD_COLOR_RED;
		case 1: 
			return LCD_COLOR_BLUE;
		case 2: 
			return LCD_COLOR_GREEN;
		case 3:
			return LCD_COLOR_MAGENTA;
		default:
			return LCD_COLOR_BLACK;
	}
}

int getTaskOffset(xTaskHandle task)
{
	int taskOffset = 0;
	if (task == t0Handle)
	{
		taskOffset = task0XPos;
	}
	else if (task == t1Handle)
	{
		taskOffset = task1XPos;
	}
	else if (task == t2Handle)
	{
		taskOffset = task2XPos;
	}
	else if (task == t3Handle)
	{
		taskOffset = task3XPos;
	}
	else if (task == t4Handle)
	{
		taskOffset = task4XPos;
	}	
	else
	{
		// should not hit this !. only 5 tasks
	}
	return taskOffset;
}

void drawStartEndTask(xTaskHandle task)
{
	int taskOffset = getTaskOffset(task);
  LCD_SetTextColor(LCD_COLOR_BLACK);
	
	LCD_DrawLine(taskOffset - 3 , lineNo-1, 5, LCD_DIR_VERTICAL);
}

void drawInTask(xTaskHandle task, PCP_MutexID id, bool lock, bool success)
{
	int taskOffset = getTaskOffset(task);
	
	int offset = 20; // width of task print
	if (lock)
	{
		offset = -5;  // before task print
	}
	
  LCD_SetTextColor(getMutexColor(id));
	
	if (success)
	{
	  LCD_DrawLine(taskOffset + offset , lineNo-1, 5, LCD_DIR_HORIZONTAL);
    LCD_DrawLine(taskOffset + offset , lineNo, 5, LCD_DIR_HORIZONTAL);
	  LCD_DrawLine(taskOffset + offset , lineNo+1, 5, LCD_DIR_HORIZONTAL);
	}
	else
	{
    LCD_DrawLine(taskOffset + offset , lineNo, 5, LCD_DIR_HORIZONTAL);
	}
}


void PCP_MutexLock(PCP_Mutex * mutex)
{
	xTaskHandle currentTask = NULL;
	xTaskHandle taskToInheritPriority = NULL;

	startTimer2();
	
	taskENTER_CRITICAL();
//	vTaskSuspendAll();
	
	currentTask = xTaskGetCurrentTaskHandle();
	
	// return if the current process is actually the 
	// holder of this PCP_Mutex. This is just in case
	// of recursive locking
	if (currentTask == mutex->taskHoldingPCP_Mutex)
	{
		stopTimer2();
		taskEXIT_CRITICAL();
		return;
	}
	
	while (getTaskPriority(currentTask) <= systemCeiling(currentTask, &taskToInheritPriority))
	{
		// find PCP_Mutex(s) blocking this task, i.e. 
		// PCP_Mutex(s) that are locked with celing >= current task priority
		// and put this current task as being "blocked" in the queue of 
		// these PCP_Mutex(s) (if not already there)
		// This is, for e.g. when two processes (with different priorities)
		// have multiple common PCP_Mutexes, and the lower priority
		// task locks more than 1 before the high priority starts
		// executing
		
		insertTaskInBlockedQueue_ofLockedPCP_Mutexs2(currentTask);
		
		//1- If Mutex is locked:
		// Give holder of this mutex the priority of the 
		// task requesting the mutex
		// dont change the original priority of the task (i.e. uxBasePCPPriority).
		// If the task requesting the mutex has higher priority (in case the 
		// task holdng the mutex was "blocked" (by suspendTask, which should not be done by the user)
		// and a lower priority task was running.
		
		//2- If Mutex is not locked:
		// the process that caused the currentSystemCeling to be highest
		// should inherit the priority of this task (if this task has higher priority).
		// this task MUST have higher priority or otherwise it wouldnt run.
		
		// for 1
		if (true == mutex->locked)
		{
			if (getTaskPriority(mutex->taskHoldingPCP_Mutex) < getTaskPriority(currentTask))
			{
				 vTaskPrioritySet(mutex->taskHoldingPCP_Mutex, getTaskPriority(currentTask));
			}
		}
		
		// for 2
		else
		{
			if (getTaskPriority(taskToInheritPriority) < getTaskPriority(currentTask))
			{
				 vTaskPrioritySet(taskToInheritPriority, getTaskPriority(currentTask));
			}
		}
		
		// Exit the critical section
		taskEXIT_CRITICAL();

		// timer to calculate time
		stopTimer2();
		// suspend the current running task, and only resume it when all PCP_Mutex(s)
		// blocking it are unlocked.
		
		// first draw that it failed to lock (blocked)
		drawInTask(currentTask, mutex->id, true, false);
		vTaskSuspend(currentTask);
		startTimer2();
		// enter the critical section after resuming the task;
		// we want to try again (go through the while loop)
		taskENTER_CRITICAL();
	}
	// current task priority > systemCeiling
	
	mutex->locked = true;
	mutex->taskHoldingPCP_Mutex = currentTask;
	mutex->nameTaskHoldingPCP_Mutex = pcTaskGetTaskName(currentTask);
	mutex->rootQueueTasksBlocked = NULL; // no blocked processes, yet
  drawInTask(currentTask, mutex->id, true, true);
	
	taskEXIT_CRITICAL();
	stopTimer2();
	return;
}

void PCP_MutexUnlock(PCP_Mutex * mutex)
{
	xTaskHandle currentTask = xTaskGetCurrentTaskHandle();
	unsigned portBASE_TYPE newTaskPriorityLevel = getBaseTaskPriority(currentTask);

	struct PCP_Mutex * loopMutex = rootOfPCP_MutexList;
	struct BlockedProcesses * tmpBlockedTask = NULL;
	struct BlockedProcesses * tmpBlockedTask_old = NULL;
	struct BlockedProcesses * tmpBlockedTaskInLoopMutex = NULL;
  bool     unlockedProcessUnique = true;
  bool     suspendedScheduler    = false;
	
	startTimer3();
	taskENTER_CRITICAL();
	
	// only current mutex can be unlocked by task holding the mutex
	if(currentTask != mutex->taskHoldingPCP_Mutex)
	{
		taskEXIT_CRITICAL();
	//	configASSERT(0);
		while(1){}
	}
		
  // check if it is this task that locked the mutex, otherwise
	// assert()
	
  // go through the list of all other mutexes still held (locked) by this task.
  // see which tasks are blocked by those "other mutexes" and give this task the 
  // highest priority in those set of tasks	(i.e. dynamic priority of this task
	// may remain the same if we are unlocking a lower priority celing mutex).
  // Note: look at the uxBasePCPPriority (original)
  // priority of those tasks
  // Note: The new priority for this task is == or < previous priority
 	
	 while ((loopMutex != NULL)  &&
	 	      (loopMutex != mutex) &&
	 		    (true == loopMutex->locked))
					{
						if (loopMutex->taskHoldingPCP_Mutex == currentTask)
						{
							//bug: was:	tmpBlockedTask = mutex->rootQueueTasksBlocked;
							tmpBlockedTask = loopMutex->rootQueueTasksBlocked;
							while(tmpBlockedTask != NULL)
							{
								// getTaskPriority returns uxPriority
								// getBaseTaskPriority returns uxBasePCPPriority
								if (getBaseTaskPriority(tmpBlockedTask->process) > getTaskPriority(currentTask))
								{
									//configASSERT(0);
									while(1){}
								}
								else
								{
									// inherit the highest priority of a task still blocked by this
									// process
									newTaskPriorityLevel = maxPriority(newTaskPriorityLevel, getBaseTaskPriority(tmpBlockedTask->process));
								}
								tmpBlockedTask = tmpBlockedTask->next;
							}
						}
						else
						{
							// If another process is holding a mutex and 
							// a different process is running (currentProcess), then currentProcess
							// must have a higher ceiling. We dont worry about inhertiting from
							// the orocess holding the other mutex.
						}
						
			      loopMutex = loopMutex->next;
					}

		// inherit the highest priority of a task still blocked by this
		// process or move this process to its Base Priority
		
		vTaskPrioritySet(currentTask, newTaskPriorityLevel);
			
					
	  // resume any unique tasks in this mutex being unlocked,
		// i.e. they are not blocked by any other mutex
	  // loop through all processes in this mutex
		// loop through all mutexes (other than blocked)
		// loop through all processes in each mutex			
		
		tmpBlockedTask = mutex->rootQueueTasksBlocked;
		loopMutex      = rootOfPCP_MutexList;
					
		// loop through all processes in this mutex
		while(tmpBlockedTask != NULL)
		{
			// loop through all mutexes locked (other than one being unlocked)
	    while ((loopMutex != NULL)  &&
	 	         (loopMutex != mutex) &&
	 		       (true == loopMutex->locked))
						 {
							 tmpBlockedTaskInLoopMutex = loopMutex->rootQueueTasksBlocked;
							 unlockedProcessUnique = true;
							 
							 // loop through all processes in each mutex			
							 while (tmpBlockedTaskInLoopMutex != NULL)
							 {
								if (tmpBlockedTask == tmpBlockedTaskInLoopMutex)
								{
									// not unique
									unlockedProcessUnique = false;
									break;
								}
								tmpBlockedTaskInLoopMutex = tmpBlockedTaskInLoopMutex->next;
							 }
							 if(unlockedProcessUnique == false)
							 {
								 // no need to check other mutexes
								 break;
							 }
							 loopMutex = loopMutex->next;
						 }

      if (unlockedProcessUnique == true)
			{
				// resume this process since now its unblocked by 
				// no other mutex
				
			  // Suspend the Scheduler (to prevent processes pre-empting, i.e. 
			  // context switch). We want to "clean-up" the mutex first, before
			  // a task is resumed and pre-empts before we set the mutex to unlocked
			  // and just call it once (since it keeps track of how many times we suspend
			  if (!suspendedScheduler)
				{
					vTaskSuspendAll();
					suspendedScheduler = true;
				}
				// This will not resume the task until the scheduler is
				// un-suspended
				vTaskResume(tmpBlockedTask->process);
				
			}
			tmpBlockedTask = tmpBlockedTask->next;
		}
		
		// remember the head of the tasks blocked list
		tmpBlockedTask_old                = mutex->rootQueueTasksBlocked;
		
		// Cleanup the mutex
		mutex->locked                     = false;
		mutex->taskHoldingPCP_Mutex       = NULL;		
	  mutex->nameTaskHoldingPCP_Mutex   = NULL;
	  mutex->rootQueueTasksBlocked      = NULL;

		drawInTask(currentTask, mutex->id, false, true);
		
		// Free memory allocated to tasks blocked by 
		// the mutex being unlocked
	  // as nothing is blocked on it now.
		// this is why we get pointer to tmpBlockedTask
		// before cleanup of mutex

    while(tmpBlockedTask_old != NULL)
		{
			tmpBlockedTask = tmpBlockedTask_old;
			tmpBlockedTask_old = tmpBlockedTask_old->next;
			free(tmpBlockedTask);
		}
		
	 // stop the timer as a context switch may happen
	// and anyways very little code is left to execute.
	stopTimer3();

	 if (suspendedScheduler == true)
		{
			// resume the scheduler
			xTaskResumeAll();
		}

	taskEXIT_CRITICAL();
	
 // when unlocking a mutex, look through all locked 
 // mutexes, and if there is a unique process in the
 // mutex being unlocked, then resume that process
 // since now its unblocked by the unlocking of 
 // the last mutex blocking it. may need to force a context
 // switch via a yield

}

PCP_Mutex * findPCP_Mutex(PCP_MutexID mutexID)
{
	struct PCP_Mutex * currentMutex = rootOfPCP_MutexList;
	while(currentMutex != NULL)
	{
		if ( mutexID == currentMutex->id)
		{
			return currentMutex;
		}
		currentMutex = currentMutex->next;
	}
	// Mutex Not Found
	//configASSERT(0);
	while(1){}
}


/* ================ Private Functions ==================== */



void insertTaskInBlockedQueue_ofLockedPCP_Mutexs2(xTaskHandle task)
{
	struct PCP_Mutex * currentMutex = rootOfPCP_MutexList;
	BlockedProcesses * endOfList    = NULL;
	while(currentMutex != NULL)
	{
		if (( true == (currentMutex->locked)) &&
			  (currentMutex->priorityCeling >= getTaskPriority(task)) &&
		    (!taskInBlockedList(currentMutex, task)))
		{

			if (currentMutex->rootQueueTasksBlocked == NULL)
			{
				currentMutex->rootQueueTasksBlocked =  (BlockedProcesses  *)calloc (1, sizeof(BlockedProcesses));
				currentMutex->rootQueueTasksBlocked->next = NULL;
				currentMutex->rootQueueTasksBlocked->process = task;
			}
			else
			{
				endOfList =  currentMutex->rootQueueTasksBlocked;
				while(endOfList->next != NULL)
				{
					endOfList = endOfList->next;
				}
				endOfList->next = (BlockedProcesses  *)calloc (1, sizeof(BlockedProcesses));
				endOfList->next->process = task;
				endOfList->next->next = NULL;
			}
		}
		currentMutex = currentMutex->next;
	}
}


// Get Priority Celing All Locked PCP_Mutexs, i.e. current system celing
unsigned portBASE_TYPE systemCeiling(xTaskHandle currentTask, xTaskHandle* taskCausingCeling)
{
	struct PCP_Mutex * currentMutex = rootOfPCP_MutexList;
	unsigned portBASE_TYPE systemPriorityCeling = 0;
	*taskCausingCeling = NULL;
	while(currentMutex != NULL)
	{
		if ((true == (currentMutex->locked)) &&
			  (currentMutex->taskHoldingPCP_Mutex != currentTask) && // This task could have locked previous mutexes
			  (currentMutex->priorityCeling > systemPriorityCeling))
		{
			systemPriorityCeling = currentMutex->priorityCeling;
			*taskCausingCeling   = currentMutex->taskHoldingPCP_Mutex;
		}
		currentMutex = currentMutex->next;
	}
	return systemPriorityCeling;
}

bool taskInBlockedList(struct PCP_Mutex * mutex, xTaskHandle task)
{
	struct BlockedProcesses * blockedProcessStruct = mutex->rootQueueTasksBlocked;
	while(blockedProcessStruct != NULL)
	{
		if (blockedProcessStruct->process == task)
		{
			return true;
		}
		blockedProcessStruct = blockedProcessStruct->next;
	}
	return false;

}

unsigned portBASE_TYPE maxPriority(unsigned portBASE_TYPE arg1, unsigned portBASE_TYPE arg2)
{
	return ((arg1 > arg2)? arg1 : arg2);
}
