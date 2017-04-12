#! /usr/bin/env python

from __future__ import print_function
import sys
import argparse
import logging

parser = argparse.ArgumentParser("Compute cluster purity.")
parser.add_argument("--ref-speakers", type=argparse.FileType('r'),
                    help="Reference list of speakers.")
parser.add_argument("mapping", type=argparse.FileType('r'),
                    help="Mapping file from md-eval.pl")

args = parser.parse_args()

ref_spk_times = {}
sys_spk_times = {}
ref_spk_to_sys = {}
spk_times = {}

i = 0
for line in args.mapping:
    if i == 0:
        i += 1
        continue
    parts = line.strip().split(',')
    ref_spk = parts[2]
    sys_spk = parts[3]
    time = float(parts[5])

    ref_spk_to_sys.setdefault(ref_spk, []).append(sys_spk)
    ref_spk_times[ref_spk] = ref_spk_times.get(ref_spk, 0.0) + time
    sys_spk_times[sys_spk] = sys_spk_times.get(sys_spk, 0.0) + time
    spk_times[(ref_spk, sys_spk)] = spk_times.get((ref_spk, sys_spk), 0.0) + time

best_spks = {}
if args.ref_speakers:
    for line in args.ref_speakers:
        parts = line.strip().split()
        best_spks[parts[0]] = "Silence"

for ref_spk, sys_spks in ref_spk_to_sys.iteritems():
    ref_time = ref_spk_times[ref_spk]
    max_overlap_time, best_spk = max([(spk_times[(ref_spk, sys_spk)], sys_spk)
                                      for sys_spk in sys_spks])
    best_spks[ref_spk] = best_spk

for ref_spk, best_spk in best_spks.iteritems():
    print ("{0} {1}".format(ref_spk, best_spk))
