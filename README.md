# priority_celing
Priority Celing Protocol Impelementation on FreeRTOS 

In this project, Priority Ceiling Protocol will be implemented as an added module to FreeRTOS. It will have public interfaces that can 
1- Create a new “PCP_Mutex”
2- Lock the “PCP_Mutex”
3- Unlock the “PCP_Mutex”.

We will highlight
1- The algorithm implementing PCP
2- Implementation details 
3- Implemented examples
4- The demonstration / test application
5-  Performance of PCP v.s regular FreeRTOS mutexes / binary semaphores (in terms of which helps “schedule” tasks better”)
6- Timing performance of PCP_Mutex  measured and analysed  (time of executing lock/unlock)

Details will be kept to minimum (no fluff) to save time for the reader, but will be enough to give them an excellent idea of what the project does and achieve. 

The Algorithm will be “summarized” in the PDF report, and the details will be available in the comments of the code.


Priority Ceiling Protocol is a scheduling algorithm that guarantees that a task will only be blocked once (at most), and no priority inversion / deadlocks /chained blocking will happen to higher priority tasks.

It is extremely useful in real time systems where high priority tasks need to run “asap” and be blocked for minimum amount of time.

This project will implement PCP as a “gate” or “mutex” type. It can also be implemented into the scheduler itself, but since we dont want to impact the original FreeRTOS code, this will be avoided. 

The report will include
1- Overview of the project
2- Design (Algorithm)
3- Examples of implementation (That have run)
4- Timing analysis
5- Conclusions.
 

Hardware Overview

STI : STM32F429I-DISCO evaluation board was used. The development environment used was keil (trial, with 32KB executable size limit). Windows was used as keil is only available in windows. 

Key Features
●	STM32F429ZIT6 microcontroller featuring 2 MB of Flash memory, 256 KB of RAM in an LQFP144 package
●	On-board ST-LINK/V2 with selection mode switch to use the kit as a standalone
●	ST-LINK/V2 (with SWD connector for programming and debugging)
●	Board power supply: through the USB bus or from an external 3 V or 5 V supply voltage
●	2.4" QVGA TFT LCD
●	SDRAM 64 Mbits
●	L3GD20, ST MEMS motion sensor, 3-axis digital output gyroscope
●	Six LEDs:
●	LD1 (red/green) for USB communication
●	LD2 (red) for 3.3 V power-on
●	Two user LEDs:LD3 (green), LD4 (red)
●	Two USB OTG LEDs:LD5 (green) VBUS and LD6 (red) OC (over-current)
●	Two pushbuttons (user and reset)
●	USB OTG with micro-AB connector
●	Extension header for LQFP144 I/Os for a quick connection to the prototyping board and an easy probing
