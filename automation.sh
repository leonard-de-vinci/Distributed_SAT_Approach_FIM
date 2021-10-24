#!/usr/local/bin/zsh

master='a'
value='a'
minSupport=('80' '60' '40' '20' '10')
nsolvers=('1' '2' '4' '8' '10' '12' '16' '20' '24')

kubectl scale deployment --all --replicas 0
kubectl scale deployment zookeeper --replicas 1
kubectl scale deployment broker --replicas 1
kubectl scale deployment schemaregistry --replicas 1
kubectl scale deployment mongodb --replicas 1
kubectl scale deployment mongoadmin --replicas 1

kubectl set env deployment/master NSOLVERS=1
kubectl set env deployment/master DATASET=../../../data/$1.dat
kubectl set env deployment/master MINSUPPORT=500
kubectl set env deployment/master RESET=0

kubectl set env deployment/slave NCORES=1
kubectl set env deployment/slave MINSUPPORT=$2

sleep 40
kubectl scale deployment master --replicas 1
sleep 3
kubectl scale deployment slave --replicas 1


master=$(kubectl get pods --no-headers -o custom-columns=":metadata.name" | grep master)

touch tests-$1.txt

kubectl scale deployment slave --replicas 0
kubectl scale deployment master --replicas 0

kubectl set env deployment/master RESET=1

for i in `seq 1 9`;
do
    kubectl set env deployment/master NSOLVERS=${nsolvers[i]}
    echo "nsolvers: ${nsolvers[i]}" >> tests-$1.txt
    for j in `seq 1 5`;
    do
        kubectl set env deployment/master MINSUPPORT=${minSupport[j]}
        kubectl set env deployment/slave MINSUPPORT=${minSupport[j]}
        sleep 40
        kubectl scale deployment master --replicas 1
        sleep 3
        kubectl scale deployment slave --replicas ${nsolvers[i]}

        echo "minSupport: ${minSupport[j]}" >> tests-$1.txt
        echo "Calculating for:\nnsolvers: ${nsolvers[i]}\nminSupport: ${minSupport[j]}"

        master=$(kubectl get pods --no-headers -o custom-columns=":metadata.name" | grep master)
        sleep 180
        value=$(kubectl logs -f $master | grep "max solving time:")
        echo "$value" >> tests-$1.txt

        kubectl scale deployment slave --replicas 0
        kubectl scale deployment master --replicas 0
    done
    echo "\n\n" >> tests-$1.txt
done