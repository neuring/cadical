#!/bin/python

from statistics import variance
import matplotlib.pyplot as plt
import sys
import re

pattern = re.compile(r"count = (?P<count>\d+), mean = (?P<mean>\d+.\d*), variance = (?P<variance>\d+.\d*), .*$")


def parse_data():
    counts = []
    means = []
    variances = []

    for data_file in sys.argv[1:]:
        with open(data_file, "r") as f:
            for line in f.readlines():
                m = pattern.match(line)
                if m is None:
                    continue
                m = m.groupdict()

                counts.append(int(m["count"]))
                means.append(float(m["mean"]))
                variances.append(float(m["variance"]))
    
    return (counts, means, variances)

counts, means, variances = parse_data()

fig, ax = plt.subplots()

print(len(counts))

ax.scatter(counts, variances)
ax.set_xlabel("counts")
ax.set_ylabel("variance")

plt.show()
