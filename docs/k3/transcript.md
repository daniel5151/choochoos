# K3 Output

## Transcript

```
Hello from the choochoos kernel!
time=10  tid=3 interval=10 completed= 1/20
time=20  tid=3 interval=10 completed= 2/20
time=23  tid=4 interval=23 completed= 1/9
time=30  tid=3 interval=10 completed= 3/20
time=33  tid=5 interval=33 completed= 1/6
time=40  tid=3 interval=10 completed= 4/20
time=46  tid=4 interval=23 completed= 2/9
time=50  tid=3 interval=10 completed= 5/20
time=60  tid=3 interval=10 completed= 6/20
time=66  tid=5 interval=33 completed= 2/6
time=69  tid=4 interval=23 completed= 3/9
time=70  tid=3 interval=10 completed= 7/20
time=71  tid=6 interval=71 completed= 1/3
time=80  tid=3 interval=10 completed= 8/20
time=90  tid=3 interval=10 completed= 9/20
time=92  tid=4 interval=23 completed= 4/9
time=99  tid=5 interval=33 completed= 3/6
time=100 tid=3 interval=10 completed=10/20
time=110 tid=3 interval=10 completed=11/20
time=115 tid=4 interval=23 completed= 5/9
time=120 tid=3 interval=10 completed=12/20
time=130 tid=3 interval=10 completed=13/20
time=132 tid=5 interval=33 completed= 4/6
time=138 tid=4 interval=23 completed= 6/9
time=140 tid=3 interval=10 completed=14/20
time=142 tid=6 interval=71 completed= 2/3
time=150 tid=3 interval=10 completed=15/20
time=160 tid=3 interval=10 completed=16/20
time=161 tid=4 interval=23 completed= 7/9
time=165 tid=5 interval=33 completed= 5/6
time=170 tid=3 interval=10 completed=17/20
time=180 tid=3 interval=10 completed=18/20
time=184 tid=4 interval=23 completed= 8/9
time=190 tid=3 interval=10 completed=19/20
time=198 tid=5 interval=33 completed= 6/6
time=200 tid=3 interval=10 completed=20/20
time=207 tid=4 interval=23 completed= 9/9
time=213 tid=6 interval=71 completed= 3/3
Goodbye from choochoos kernel!
```

## Explanation

Each client task displays the current time as it executes. The `time` should
always equal `interval * num_completed`, which holds for every line in the
transcript.  This means that when multiple tasks are blocked for different
amounts of time, the tasks are woken up in the right order as early as
possible.

We wrote a python script in `test/k3_expected.py` that calculates the exact
output of the `k3` tasks (based on the above formula), and the output of the
python script matches our execution transcript exactly.

Idle measurements are printed to the top right of the screen in real-time.
However, since these measurements are not deterministic, they are omitted from
the transcript.
