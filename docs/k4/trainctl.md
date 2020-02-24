# A0 Redux - Now with 100% more Kernel!

Overview
--------

It's been almost two months since A0, and we're now half way through the journey of self-discovery and sleep-deprivation that is CS 452. The time has finally come to be reunited with the venerable Märklin 6051 train set, and get those trains chuggin' along.

Unlike A0 though, this time we've got an actual OS to build on top of!

Using our newfound _OS and embedded programming skillz_, we've (re)implemented a simple program to control various aspects of the model trainset, including but not limited to: setting the speed of individual trains, reversing individual trains, and setting various switches on the track to be either straight or curved. This program simultaneously displays the time since startup, and a list of recently triggered track sensors.

Operating Instruction
---------------------

After a brief initialization procedure, the display should look something similar to the following:

```
[0:23:3]                                                 [Idle Time 97%]

Sensors: D11

Train |   1 |  24 |  58 |  74 |  78 |  79 |
Speed |   0 |   0 |   0 |   0 |   0 |   0 |

Switches
|  1 C|  2 C|  3 C|  4 C|  5 C|  6 C|  7 C|  8 C|
|  9 C| 10 S| 11 C| 12 C| 13 S| 14 C| 15 C| 16 S|
| 17 S| 18 C|153 C|154 C|155 C|156 C|





Initializing Track...
Ready.
>
```

The display consists of the following components:

- A live updating timer, displaying the amount of time the program has been running for
- A readout of the system's reported idle time
- A list of (valid) trains with their current speeds
- A list of (valid) switches with their current state (**C**urved or **S**traight)
- A readout of the track's recently triggered sensors
- A command prompt
- (**extra**) A feedback message regarding the last run / currently running command

The program accepts all the commands specified in the A0 assignment outline:

Command                                 | Description
----------------------------------------|--------------------------------------------------------------
`tr <train number> <train speed>`       | Sets the speed of a given train (from 1 - 14, with 0 to stop)
`rv <train number>`                     | Reverses the direction of a given train
`sw <switch number> <switch direction>` | Sets a given track switch to be either (s)traight or (c)urved
`q`                                     | Quit the program and return back to RedBoot

In addition to the required commands, this program also implements a few additional "extra" commands:

Command            | Description
-------------------|------------
`l <train number>` | Toggles the lights of a given train
`s`                | Sends the "Emergency Stop" (i.e: STOP) signal to the train set
`g`                | Sends the "Release" (i.e: GO) signal to the train set

As a quality-of-life feature, the program remembers the last-entered command, which is re-run if the return key is hit without entering a new command. This is most useful in conjunction with the `l` command, as it lets you blink a train's lights on-and-off! How Fun!

Architecture
------------

Unlike A0, we actually have access to an OS for this assignment! This has made it a lot easier to implement much of the program's functionality, most notably in relation to task scheduling and timing.

Broadly speaking, for each UI element on screen (e.g: command prompt, train speed, sensors, etc...), there is a corresponding Task which performs any data processing and/or IO required to render the information to the screen.

In total, we use 6 application-specific long-running tasks to run this application:

- PerfTask
    - Uses the `Perf` syscall to render the idle time in the top right corner of the screen
- TimerTask
    - Uses the Clock Server to display a application uptime timer in the top right corner of the screen
- CmdTask
    - Renders the Command prompt (`> ...`) and command feedback UI
    - Reads lines from UART2, parses them as commands, and relays requests to the MarklinCommandTask
    - Train commands update the train UI, while the switch command updates the switch UI
- MarklinCommandTask
    - Encapsulates the nitty-gritty details of communicating with the Marklin controller
    - Accepts high-level requests (i.e: set train X to have speed Y), and translates them into specific byte-sequences to be output via UART1
- SensorPollerTask + SensorReporterTask
    - SensorPollerTask sends a Sensor Query request to the MarklinCommandTask every 300ms
    - SensorReporterTask reads incoming sensor data from UART1, displaying the data via the sensors display

Known Bugs
----------

- User input doesn't work while train commands are being executed.
    - This is most noticeable with the `rv` command, as is implemented inline in the cmd task.
    - One potential fix would be to have a separate "ReverseManager" task which would accept reverse requests from the cmd task.
