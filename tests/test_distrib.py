#!/usr/local/bin/python3
# -*- Encoding: utf-8 -*-
# Authors: Julien MARTIN-PRIN

import subprocess
import os
import re
import sys

#nSolvers = [1, 2, 4, 8, 10, 12, 16, 20, 24]
nSolvers = [1, 2]

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


def main():
    send_time = ""
    if len(sys.argv) < 4:
        print("Usage: scpript.py dataset minSupport_init minSupport1 ... minSupportn")
        exit
    dataset = sys.argv[1]
    minSupport_init = int(sys.argv[2])
    minSupport = []
    for i in range(3, len(sys.argv)):
        minSupport.append(int(sys.argv[i]))
    print("Initializing...")
    send_time = init(dataset, minSupport_init)
    print("Initialized")
    test = open(f"tests-{dataset}.csv", "w")
    test.write("nsolvers;minsupport;time_elapsed;send_time\n")
    test.close()
        
    for i, n in enumerate(nSolvers):
        test = open(f"tests-{dataset}.csv", "a")
        subprocess.run(['kubectl', 'set', 'env', 'deployment/master', f'NSOLVERS={n}'])
        for j, support in enumerate(minSupport):
            subprocess.run(['kubectl', 'set', 'env', 'deployment/master', f'MINSUPPORT={support}'])
            subprocess.run(['kubectl', 'scale', 'deployment', 'master', '--replicas', '1'])
            
            master = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
            master = str(master.communicate()).split("\\n")

            r = re.compile("master.*")
            master = list(filter(r.match, master))

            while(len(master) != 1):
                master = subprocess.Popen(['kubectl', 'get', 'pods', '--no-headers'], stdout = subprocess.PIPE)
                master = str(master.communicate()).split("\\n")
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

            subprocess.run(['kubectl', 'scale', 'deployment', 'slave', '--replicas', f'{n}'])

            print(f"Calculating for:\nnsolvers: {n}\nminSupport: {support}")
            
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
            if j == 0:
                if i == 0:
                    test.write(f"{n};{support};{value};{send_time}\n")
                else:
                    test.write(f"{n};{support};{value};\n")
            else:
                test.write(f";{support};{value};\n")
            print(value)
            subprocess.run(['kubectl', 'scale', 'deployment', 'slave', '--replicas', '0'])
            subprocess.run(['kubectl', 'scale', 'deployment', 'master', '--replicas', '0'])
        test.write(";;;\n")
        test.close()

if __name__ == '__main__':
    main()