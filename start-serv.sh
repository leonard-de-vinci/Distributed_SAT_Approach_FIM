kubectl scale deployment zookeeper --replicas 1
kubectl scale deployment broker --replicas 1
kubectl scale deployment schemaregistry --replicas 1
kubectl scale deployment mongodb --replicas 1
kubectl scale deployment mongoadmin --replicas 1