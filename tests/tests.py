#!/usr/local/bin/python3
# -*- Encoding: utf-8 -*-
# Authors: Julien MARTIN-PRIN

import subprocess
import os
import re
import sys

#nSolvers = [1, 2, 4, 8, 10, 12, 16, 20, 24]
nSolvers = [1, 4, 8, 16, 24]

def getMinSupport():
    n = int(input("Number of minSupport: "))
    minSupport = list()
    for i in range(n):
        minSupport.append(int(input("minSupport: ")))
    return minSupport

def init(dataset, minSupport):
    send_time = ""
    subprocess.run(['kubectl', 'scale', 'deployment', '--all', '--replicas', '0'])
    subprocess.run(['kubectl', 'scale', 'deployment', 'zookeeper', '--replicas', '1'])
    subprocess.run(['kubectl', 'scale', 'deployment', 'broker', '--replicas', '1'])
    subprocess.run(['kubectl', 'scale', 'deployment', 'schemaregistry', '--replicas', '1'])
    subprocess.run(['kubectl', 'scale', 'deployment', 'mongodb', '--replicas', '1'])
    subprocess.run(['kubectl', 'scale', 'deployment', 'mongoadmin', '--replicas', '1'])
    subprocess.run(['kubectl', 'set', 'env', 'deployment/master', 'NSOLVERS=1'])
    subprocess.run(['kubectl', 'set', 'env', 'deployment/master', f'DATASET=../../../data/{dataset}.dat'])
    subprocess.run(['kubectl', 'set', 'env', 'deployment/master', f'MINSUPPORT={minSupport}'])
    subprocess.run(['kubectl', 'set', 'env', 'deployment/master', 'RESET=0'])
    subprocess.run(['kubectl', 'set', 'env', 'deployment/slave', 'NCORES=1'])
    subprocess.run(['kubectl', 'scale', 'deployment', 'master', '--replicas', '1'])

    master = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
    master = str(master.communicate()).split("\\n")

    r = re.compile("master.*")
    master = list(filter(r.match, master))

    while(len(master) != 1):
        master = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
        master = str(master.communicate()).split("\\n")
        master = list(filter(r.match, master))

    status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
    status = str(status.communicate()).split("\\n")
    status = list(filter(r.match, status))[0].split(" ")
    r = re.compile(".*Running.*")
    status = list(filter(r.match, status))

    while(status == []):
        status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
        status = str(status.communicate()).split("\\n")
        r = re.compile("master.*")
        status = list(filter(r.match, status))[0].split(" ")
        r = re.compile(".*Running.*")
        status = list(filter(r.match, status))

    subprocess.run(['kubectl', 'scale', 'deployment', 'slave', '--replicas', '1'])
    
    master = master[0].split(" ")[0]

    value = ""
    r = re.compile("Terminating.*")

    while(value != "Terminating..."):
        value = subprocess.Popen(f'kubectl logs {master} | grep "Terminating..."', shell = True, stdout = subprocess.PIPE)
        value = str(value.communicate()).split("'")
        value = list(filter(r.match, value))
        if (value != []):
            value = value[0].split("\\n")[0]

    send_time = subprocess.Popen(f'kubectl logs {master} | grep "Data sending time:"', shell = True, stdout = subprocess.PIPE)
    send_time = str(send_time.communicate()).split("'")
    r = re.compile("Data sending time:.*")
    send_time = list(filter(r.match, send_time))[0].split("\\n")[0].split(" ")[3]
        
    subprocess.run(['kubectl', 'scale', 'deployment', 'slave', '--replicas', '0'])
    subprocess.run(['kubectl', 'scale', 'deployment', 'master', '--replicas', '0'])
    subprocess.run(['kubectl', 'set', 'env', 'deployment/master', 'RESET=1'])

    return send_time



def test_parallel(minSupport, dataset):
    print("Initializing...")
    subprocess.run(['kubectl', 'set', 'env', 'deployment/parallel', f'DATASET=../../../data/{dataset}.dat'])
    print("Initialized")
    results = []
        
    for i, n in enumerate(nSolvers):
        subprocess.run(['kubectl', 'set', 'env', 'deployment/parallel', f'NCORES={n}'])
        results.append([])
        for j, support in enumerate(minSupport):
            subprocess.run(['kubectl', 'set', 'env', 'deployment/parallel', f'MINSUPPORT={support}'])
            subprocess.run(['kubectl', 'scale', 'deployment', 'parallel', '--replicas', '1'])
            
            parallel = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
            parallel = str(parallel.communicate()).split("'")

            r = re.compile(".*parallel.*")
            parallel = list(filter(r.match, parallel))[0].split("\\n")
            r = re.compile("parallel.*")
            parallel = list(filter(r.match, parallel))
            
            while(len(parallel) != 1):
                parallel = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
                parallel = str(parallel.communicate()).split("'")
                r = re.compile(".*parallel.*")
                parallel = list(filter(r.match, parallel))[0].split("\\n")
                r = re.compile("parallel.*")
                parallel = list(filter(r.match, parallel))
            
            parallel = parallel[0].split(" ")[0]
            
            status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
            status = str(status.communicate()).split("\\n")
            r = re.compile(".*parallel.*")
            status = list(filter(r.match, status))[0].split(" ")
            r = re.compile(".*Running.*")
            status = list(filter(r.match, status))


            while(status == []):
                status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
                status = str(status.communicate()).split("\\n")
                r = re.compile(".*parallel.*")
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
            results[i].append(float(value))
            subprocess.run(['kubectl', 'scale', 'deployment', 'parallel', '--replicas', '0'])
    return results

def test_distrib(minSupport_init, minSupport, dataset):
    send_time = ""
    print("Initializing...")
    send_time = init(dataset, minSupport_init)
    print("Initialized")

    results = []
        
    for i, n in enumerate(nSolvers):
        subprocess.run(['kubectl', 'set', 'env', 'deployment/master', f'NSOLVERS={n}'])
        results.append([])
        for j, support in enumerate(minSupport):
            subprocess.run(['kubectl', 'set', 'env', 'deployment/master', f'MINSUPPORT={support}'])
            subprocess.run(['kubectl', 'scale', 'deployment', 'master', '--replicas', '1'])
            
            master = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
            master = str(master.communicate()).split("'")

            r = re.compile(".*master.*")
            master = list(filter(r.match, master))[0].split("\\n")
            r = re.compile("master.*")
            master = list(filter(r.match, master))

            while(len(master) != 1):
                master = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
                master = str(master.communicate()).split("'")
                r = re.compile(".*master.*")
                master = list(filter(r.match, master))[0].split("\\n")
                r = re.compile("master.*")
                master = list(filter(r.match, master))
            
            master = master[0].split(" ")[0]
            
            status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
            status = str(status.communicate()).split("\\n")
            status = list(filter(r.match, status))[0].split(" ")
            r = re.compile(".*Running.*")
            status = list(filter(r.match, status))

            while(status == []):
                status = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
                status = str(status.communicate()).split("\\n")
                r = re.compile("master.*")
                status = list(filter(r.match, status))[0].split(" ")
                r = re.compile(".*Running.*")
                status = list(filter(r.match, status))

            subprocess.run(['sleep', '5'])

            subprocess.run(['kubectl', 'scale', 'deployment', 'slave', '--replicas', f'{n}'])
            
            value = ""
            r = re.compile("Terminating.*")
            
            while(value != "Terminating..."):
                value = subprocess.Popen(f'kubectl logs {master} | grep "Terminating..."', shell = True, stdout = subprocess.PIPE)
                value = str(value.communicate()).split("'")
                value = list(filter(r.match, value))
                if (value != []):
                    value = value[0].split("\\n")[0]

            value = subprocess.Popen(f'kubectl logs {master} | grep "max solving time:"', shell = True, stdout = subprocess.PIPE)
            value = str(value.communicate()).split("'")
            r = re.compile("max solving time:.*")
            value = list(filter(r.match, value))[0].split("\\n")[0]
            value = value.split(" ")[3]
            results[i].append(float(value))
            subprocess.run(['kubectl', 'scale', 'deployment', 'slave', '--replicas', '0'])
            subprocess.run(['kubectl', 'scale', 'deployment', 'master', '--replicas', '0'])

    subprocess.run(['kubectl', 'scale', 'deployment', '--all', '--replicas', '0'])
    return results, send_time

def main():
    if len(sys.argv) < 4:
        print("Usage: scpript.py dataset minSupport_init minSupport1 ... minSupportn")
        exit
    dataset = sys.argv[1]
    minSupport_init = int(sys.argv[2])
    minSupport = []
    for i in range(3, len(sys.argv)):
        minSupport.append(int(sys.argv[i]))
    results_distrib, send_time = test_distrib(minSupport_init, minSupport, dataset)
    results_para = test_parallel(minSupport, dataset)

    test = open(f"{dataset}.csv", "w")
    test.write("minSupport;DSATminer-1s;PSATminer-1c;DSATminer-4s;PSATminer-4c;DSATminer-8s;PSATminer-8c;DSATminer-16s;PSATminer-16c;DSATminer-24s;PSATminer-24c;Send_time\n")
    test.close()

    test = open(f"{dataset}.csv", "a")

    for i, support in enumerate(minSupport):
        test.write(f"{support};")
        for j in range(len(nSolvers)):
            test.write(f"{results_distrib[j][i]};{results_para[j][i]};")
        if i == 0:
            test.write(f"{send_time}")
        test.write("\n")
    
    test.close()

if __name__ == '__main__':
    main()