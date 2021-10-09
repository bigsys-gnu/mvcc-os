#!/sh
# threadnum, init_node, bucket, time, update ratio, range

bucket=1000
init_node=5000
val_range=10000
duration=10000

for sync in rcu rlu mvrlu spin; do
    for thread in 1 5 10 15; do
        for update in 20 200 500; do
            echo "-- Start a benchmark --" 
            echo hlbench_$sync $thread $init_node $bucket $duration $update $val_range
            hlbench_$sync $thread $init_node $bucket $duration $update $val_range
        done
    done
done 
