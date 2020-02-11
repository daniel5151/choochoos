from collections import namedtuple

Task = namedtuple("Task", ["tid", "delay", "num_delays"])
Event = namedtuple("Event", ["task", "time", "delays_completed"])

tasks = [Task(4, 10, 20), Task(5, 23, 9), Task(6, 33, 6), Task(7, 71, 3)]

events = [
    Event(task, (i + 1) * task.delay, i + 1)
    for task in tasks
    for i in range(task.num_delays)
]

print("Hello from the choochoos kernel!")
events.sort(key=lambda evt: [evt.time, -evt.task.tid])
for event in events:
    print(
        "time=%-3d tid=%d interval=%-2d completed=%2d/%d"
        % (
            event.time,
            event.task.tid,
            event.task.delay,
            event.delays_completed,
            event.task.num_delays,
        )
    )
print("Goodbye from choochoos kernel!")
