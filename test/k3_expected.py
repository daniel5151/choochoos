from collections import namedtuple

Task = namedtuple("Task", ["priority", "delay", "num_delays"])
Event = namedtuple("Event", ["task", "time", "delays_completed"])

tasks = [Task(3, 10, 20), Task(4, 23, 9), Task(5, 33, 6), Task(6, 71, 3)]

events = [
    Event(task, (i + 1) * task.delay, i + 1)
    for task in tasks
    for i in range(task.num_delays)
]

events.sort(key=lambda evt: [evt.time, -evt.task.priority])
for event in events:
    print(
        "time=%-3d tid=%d interval=%-2d completed=%-2d"
        % (event.time, event.task.priority, event.task.delay, event.delays_completed)
    )
