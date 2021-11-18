#!/usr/local/bin/python3
# -*- Encoding: utf-8 -*-
# Authors: Julien MARTIN-PRIN

import subprocess
import os
import re
import sys

#nSolvers = [1, 2, 4, 8, 10, 12, 16, 20, 24]
nCores = [1, 2]

def getMinSupport():
    n = int(input("Number of minSupport: "))
    minSupport = list()
    for i in range(n):
        minSupport.append(int(input("minSupport: ")))
    return minSupport

def main():
    if len(sys.argv) < 3:
        print("Usage: scpript.py dataset minSupport1 ... minSupportn")
        exit
    dataset = sys.argv[1]
    minSupport = []
    for i in range(2, len(sys.argv)):
        minSupport.append(int(sys.argv[i]))
    print("Initializing...")
    subprocess.run(['kubectl', 'set', 'env', 'deployment/parallel', f'DATASET=../../../data/{dataset}.dat'])
    print("Initialized")
    test = open(f"tests-{dataset}-parallel.csv", "w")
    test.write("ncores;minsupport;time_elapsed\n")
    test.close()
        
    for n in nCores:
        test = open(f"tests-{dataset}-parallel.csv", "a")
        subprocess.run(['kubectl', 'set', 'env', 'deployment/parallel', f'NCORES={n}'])
        for i, support in enumerate(minSupport):
            subprocess.run(['kubectl', 'set', 'env', 'deployment/parallel', f'MINSUPPORT={support}'])
            subprocess.run(['kubectl', 'scale', 'deployment', 'parallel', '--replicas', '1'])
            
            parallel = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
            parallel = str(parallel.communicate()).split("\\n")

            r = re.compile("parallel.*")
            parallel = list(filter(r.match, parallel))

            while(len(parallel) != 1):
                parallel = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
                parallel = str(parallel.communicate()).split("\\n")
                parallel = list(filter(r.match, parallel))
            
            parallel = parallel[0].split(" ")[0]
            
            status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
            status = str(status.communicate()).split("\\n")
            status = list(filter(r.match, status))[0].split(" ")
            r = re.compile(".*Running.*")
            status = list(filter(r.match, status))

            while(status == []):
                status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
                status = str(status.communicate()).split("\\n")
                r = re.compile("parallel.*")
                status = list(filter(r.match, status))[0].split(" ")
                r = re.compile(".*Running.*")
                status = list(filter(r.match, status))

            value = ""
            r = re.compile("Terminating.*")
            
            while(value != "Terminating..."):
                value = subprocess.Popen(f'kubectl logs {parallel} | grep "Terminating..."', shell = True, stdout = subprocess.PIPE)
                value = str(value.communicate()).split("'")
                value = list(filter(r.match, value))
                if (value != []):
                    value = value[0].split("\\n")[0]

            value = subprocess.Popen(f'kubectl logs {parallel} | grep "time elapsed:"', shell = True, stdout = subprocess.PIPE)
            value = str(value.communicate()).split("'")
            r = re.compile("time elapsed:.*")
            value = list(filter(r.match, value))[0].split("\\n")[0]
            value = value.split(" ")[2]
            if i == 0:
                test.write(f"{n};{support};{value}\n")
            else:
                test.write(f";{support};{value}\n")
            subprocess.run(['kubectl', 'scale', 'deployment', 'parallel', '--replicas', '0'])
        test.write(";;;\n")
        test.close()

if __name__ == '__main__':
    main()