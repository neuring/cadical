#!/bin/python
from typing import List, Tuple
import matplotlib.pyplot as plt
import regex as re
import sys

r = re.compile(r"c cls = \[(?P<cls>(?:-?\d+,)+)\], count = (?P<count>\d+), mean = (?P<mean>\d+.\d+), variance = (?P<variance>\d+.\d+)")

class ParseResult:
    def __init__(self, cls: List[int], count: int, mean: float, variance: float):
        self.cls = cls
        self.count = count
        self.mean = mean
        self.variance = variance


def parse_data(file):
    with open(file, "r") as f:
        for line in f.readlines():
            m = r.match(line.strip())
            assert m is not None
            parts = m.groupdict()

            cls = [int(lit.strip()) for lit in parts["cls"].split(",") if lit != ""]
            assert sorted(cls) == cls, f"{cls}"
            count = int(parts["count"])
            mean = float(parts["mean"])
            variance = float(parts["variance"])
            yield ParseResult(cls, count, mean, variance)

# Calculation for cominding mean and variance taken from:
# https://www.emathzone.com/tutorials/basic-statistics/combined-variance.html
def combine_data(d1: ParseResult, d2: ParseResult) -> ParseResult:
    assert d1.cls == d2.cls
    combined_count = d1.count + d2.count
    combined_mean = (d1.mean * d1.count + d2.mean * d2.count) / combined_count
    combined_variance = (d1.count * (d1.variance + (d1.mean  - combined_mean)) + d2.count * (d2.variance + (d2.mean - combined_mean))) / combined_count
    return ParseResult(d1.cls, combined_count, combined_mean, combined_variance)

def accumulate_data(file):
    data = {}
    for d in parse_data(file):
        tuple_cls = tuple(d.cls)
        if tuple_cls in data:
            data[tuple_cls] = combine_data(data[tuple_cls], d)
        else:
            data[tuple_cls] = d
    return data

file = sys.argv[1]
d = accumulate_data(file)
for k, v in d.items():
    print(f"{v.cls}: count={v.count}, mean={v.mean}, variance={v.variance}")
