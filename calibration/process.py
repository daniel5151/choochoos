import json
import os
import sys
from collections import namedtuple
from pprint import pprint
from parse import parse
import matplotlib.pyplot as plt
import networkx as nx

TrainTrack = namedtuple("TrainTrack", ["train", "track"])

dirname = os.path.dirname(sys.argv[0])


def parse_train_data(fname):
    with open(fname) as fp:
        return json.load(fp)


def datafile(tt):
    return "{}/raw/{}-{}.json".format(dirname, tt.train, tt.track)


def reverse(edge):
    return (edge[1], edge[0])


def is_sensor(sensor):
    res = parse("{}{:d}", sensor)
    assert res
    return len(res[0]) == 1


def other_sensor(sensor):
    res = parse("{}{:d}", sensor)
    assert res
    letter, idx = res
    other_idx = idx - 1 if idx % 2 == 0 else idx + 1
    return "{}{:d}".format(letter, other_idx)


def parse_track(fname):
    lines = open(fname).readlines()

    graph = nx.DiGraph()
    for line in lines:
        line = line.strip()
        res = parse("NODE name={} type={} num={:d} rev={}", line)
        if res:
            graph.add_node(res[0])
            continue
        res = parse("EDGE src={} dest={} dist={:d}", line)
        if res:
            graph.add_edge(res[0], res[1], weight=res[2], label=res[2])
            continue
        raise Exception("could not parse line '{}'".format(line))

    return graph


def process(tt, track_graph):
    data = parse_train_data(datafile(tt))
    speed = None
    last_sensor_evt = None

    sensors_by_speed = {}
    start_time = None

    assert data["train"] == tt.train
    for evt in data["events"]:
        if evt["event"] == "speed":
            speed = evt["val"]
            last_sensor_evt = None
            start_time = evt["time"]
            sensors_by_speed[speed] = []
        else:
            assert evt["event"] == "sensor"
            if last_sensor_evt is None:
                last_sensor_evt = evt
                continue

            sensor = evt["sensor"]
            time = evt["time"]

            if sensor != last_sensor_evt["sensor"]:
                from_sensor = last_sensor_evt["sensor"]
                dist = nx.dijkstra_path_length(track_graph, from_sensor, sensor)
                dt = time - last_sensor_evt["time"]
                avg_velocity = dist / (dt / 508000)
                sensors_by_speed[speed].append(
                    {
                        "from": from_sensor,
                        "to": sensor,
                        "start_time": last_sensor_evt["time"] - start_time,
                        "end_time": time - start_time,
                        "distance_mm": dist,
                        "avg_vel_mmps": avg_velocity,
                    }
                )
            last_sensor_evt = evt
    return sensors_by_speed


def plot_train_speeds(tt, sensors_by_speed):
    for speed, data in sensors_by_speed.items():
        x = []
        y = []
        pprint((speed, data))
        if len(data) <= 2:
            continue
        for item in data[1:]:
            x.append(item["end_time"] / 508000)
            y.append(item["avg_vel_mmps"])
        plt.plot(x, y, label=speed)
    plt.title("Train {}, Track {}".format(tt.train, tt.track))
    plt.ylabel("average velocity (mm/s)")
    plt.xlabel("time (s)")
    plt.ylim(bottom=0)
    plt.xlim(left=0)
    plt.legend(title="Speed level")
    plt.show()


def main():
    track_graph = parse_track(os.path.join(dirname, "track_a.txt"))
    nx.drawing.nx_agraph.write_dot(track_graph, os.path.join(dirname, "track_a.dot"))

    trains = [1, 24, 58, 74, 78, 79]
    sensor_data = {}
    for train in trains:
        tt = TrainTrack(train, "a")
        sensor_data[train] = process(tt, track_graph)
        # plot_train_speeds(tt, sensors_by_speed)

    for speed in [13, 8]:
        speeds = []
        labels = []
        for train in trains:
            sensors_by_speed = sensor_data[train]
            pprint(sensors_by_speed.keys())
            speeds.append([x["avg_vel_mmps"] for x in sensors_by_speed[speed][1:]])
            labels.append("train {}\nspeed {}".format(train, speed))
        plt.title(
            "Train speed distributions: track {}, speed level {}".format(
                tt.track, speed
            )
        )
        plt.grid(axis="y", which="both")
        plt.ylabel("observed speed\nmm/s")
        plt.boxplot(speeds, labels=labels)
        plt.show()


if __name__ == "__main__":
    main()
