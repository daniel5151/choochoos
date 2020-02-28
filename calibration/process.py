import json
import os
import sys
from collections import namedtuple

TrainTrack = namedtuple("TrainTrack", ["train", "track"])


def parse(fname):
    with open(fname) as fp:
        return json.load(fp)


def datafile(tt):
    dirname = os.path.dirname(sys.argv[0])
    return "{}/raw/{}-{}.json".format(dirname, tt.train, tt.track)


def process(tt):
    data = parse(datafile(tt))
    speed = None
    last_sensor = None
    sensors_by_speed = {}
    start_time = None

    assert data["train"] == tt.train
    for evt in data["events"]:
        if evt["event"] == "speed":
            speed = evt["val"]
            last_sensor = None
            start_time = evt["time"]
            sensors_by_speed[speed] = []
        else:
            assert evt["event"] == "sensor"


process(TrainTrack(1, "a"))
