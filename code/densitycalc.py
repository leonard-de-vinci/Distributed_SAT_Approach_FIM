#!/usr/local/bin/python3
# -*- Encoding: utf-8 -*-
# Authors: Julien MARTIN-PRIN

import os
import sys

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 script.py dataset #items")
        exit()
    path = "/Users/Flexiboy/Desktop/Projects/Distributed_SAT_Approach_FIM/data/" + sys.argv[1] + ".dat"
    summ = 0
    nitems = int(sys.argv[2])
    ntrans = 0

    with open(path, "r") as inp:
        for line in inp:
            summ += len(line.split(" ")) - 1
            ntrans +=1

    density = float(summ) / (float(nitems) * float(ntrans)) * 100
    
    print(f"Density: {density}%")

if __name__ == "__main__":
    main()