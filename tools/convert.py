#!/usr/bin/env python3
import argparse
import os
import tempfile
import matplotlib.pyplot as plt
import numpy as np

parser = argparse.ArgumentParser(description='Convert trace file to svg')
parser.add_argument('file', type=str, help='Path to trace file')
parser.add_argument('folder', type=str, help='Path to folder where files will be placed')

args = parser.parse_args()

names = []
maxtimestamp = 0

# Generate timestamps and names array
f = open(args.file, "r")
for x in f:
  if " " in x:
    row = x.split()
    if len(row) == 3:
      ts = row[0]
      level = row[1]
      name = row[2]
      found = False
      if int(ts) > maxtimestamp:
        maxtimestamp = int(ts)
      for n in names:
        if n == name:
          found = True
          break
      if not found:
        names.append(name)
f.close()

if not os.path.exists(args.folder):
  os.makedirs(args.folder)
tmpf = open(args.folder + "/index.html", "w")
tmpf.write("<html><body>\n")

for k in names:
  fig, ax = plt.subplots()
  plt.clf()
  fig.set_size_inches(maxtimestamp / 100000, 3)
  plt.xlim(-1, maxtimestamp + 1000000)

  plt.ylim(-0.5, 1.5)

  plt.xlabel('time (usec)')
  plt.ylabel('on / off')
  #plt.title(k)
  ax.grid()

  levels = []
  timestamps = []
  lastlevel = 0
  first = True

  f = open(args.file, "r")
  for x in f:
    if " " in x:
      row = x.split()
      if len(row) == 3:
        ts = row[0]
        level = row[1]
        name = row[2]
        if name != k:
          continue
        if first:
          first = False
          if level == "0":
            levels.append(0)
            lastlevel = 0
          else:
            levels.append(1)
            lastlevel = 1
          timestamps.append(0)

        timestamps.append(int(ts))
        levels.append(lastlevel)

        timestamps.append(int(ts))
        if level == "0":
          levels.append(0)
          lastlevel = 0
        else:
          levels.append(1)
          lastlevel = 1
  timestamps.append(maxtimestamp + 1000000)
  levels.append(lastlevel)

  plt.plot(timestamps, levels)

  for i in range(0, maxtimestamp, 1000000):
    timestamps.append(i)
    plt.text(i, 1.25, k, fontsize=12)

  plt.xticks(timestamps)
  plt.grid(True)

  fig.tight_layout()
  fig.set_dpi(50)
  f.close()
  plt.savefig(args.folder + "/" + k + ".svg")
  plt.close(fig)
  tmpf.write("<img src=\""+ k +".svg\" title=\"" + k +"\"><br>\n")
  print(k + ".svg")

tmpf.write("</html></body>\n")
tmpf.close()
