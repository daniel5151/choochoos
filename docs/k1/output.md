# K1 Output

## Transcript

```
1  | Created: 1
2  | Created: 2
3  | MyTid=3 MyParentTid=0
4  | MyTid=3 MyParentTid=0
5  | Created: 3
6  | MyTid=3 MyParentTid=0
7  | MyTid=3 MyParentTid=0
8  | Created: 3
9  | FirstUserTask: exiting
10 | MyTid=1 MyParentTid=0
11 | MyTid=2 MyParentTid=0
12 | MyTid=1 MyParentTid=0
13 | MyTid=2 MyParentTid=0
```

## Explanation

- Lines 1-2
    - `FirstUserTask` calls `Create(3, OtherTask)` twice, and is immedately re-scheduled both times, printing out the two lines. This stems from the fact that `FirstUserTask` is spawned with a priority of `4`, which is a higher priority than the two `OtherTask`s it spawned. As such, the scheduler kept re-prioritizing the `FirstUserTask` for execution.
- Lines 3-5
    - When `FirstUserTask` calls `Create(5, OtherTask)`, instead of immediately being rescheduled, the newly-created `OtherTask` is run instead, as it was spawned at a higher priority than the `FirstUserTask`.
    - When `OtherTask` calls the `MyTid` syscall, it gets back a value of 3. This makes sense, as we use a simple increasing Tid counter (starting at 0) when spawning tasks. As this task is the 4rd task to have been spawned by the system, it was assigned a Tid of 3.
    - When `OtherTask` calls the `MyParentTid` syscall, it gets back a value of 0. This is as expected, as it's parent task is `FirstUserTask`, which is always assigned the Tid of 0.
    - Even though the `OtherTask` yields between it's two print statements, at the time that it's yielded, it remains as the highest priority task on the system, and is immediately rescheduled.
    - Only once the `OtherTask` completes does execution return to the `FirstUserTask`, at which point it prints out "Created: 3"
- Lines 6-8
    - These lines are identical to lines 3-5, as once the first high-priority `OtherTask` calls exit, it's Tid is returned to the free Tid pool, which is then re-used for the newly spawned `OtherTask`.
- Lines 9-13
    - Once the second high-priority `OtherTask` has completed, and execution has returned to the `FirstUserTask`, the `FirstUserTask` is done, and calls exits itself. At this point, there are only two remaining tasks left on the system: The two low-priority `OtherTasks`s
    - Since both tasks have the same priority, they are scheduled in a round-robin fashion, which results in the interleaved output between the two tasks. When the first `OtherTask` prints it's line, it immediately yields, allowing the second `OtherTask` to print it's line. The second `OtherTask` then yields, allowing the first `OtherTask` to print it's line (and so on and so forth).
    - NOTE: the two tasks yield execution during the `MyTid` and `MyParentTid` calls as well, interweaving execution even further. That being said, since there is no output between these syscall invocations, it's more difficult to see the interwoven execution flow.
