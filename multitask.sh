#!/bin/bash
TEMP=$(getopt -o hc:t: --long help,coins:,tasks: -n 'multiltask.sh' -- "$@")

if [ $? -ne 0 ]; then >&2 ; exit 1; fi

eval set -- "$TEMP"

usage(){
    echo "-h, --help"
    echo "      show this message"
    echo "-c, --coins x"
    echo "      set number of coins (depreciated)"
    echo "-t, --tasks x"
    echo "      set number of tasks to start default is 4"
}

coins=440
tasks=4

while true; do
    case "$1" in
        -h | --help) usage ; exit ;;
        -c | --coins) coins=$2 ; shift 2 ;;
        -t | --tasks) tasks=$2 ; shift 2 ;;
        --) shift ; break;;
        *) break;;
    esac
done


/bin/time -f "%E" ./get_data.out --get-exchange-info --verbose
coins=$(./get_data.out --has-exchange-info --get-number-of-coins)

part=$(expr $coins / $tasks)
i=1
c=0
while [ $i -le $tasks ]
do
    /bin/time -f "task ${i} %E" ./get_data.out --has-exchange-info --bfile ${i}.out --hfile ${i}h.out --csvfile ${i}.csv --no-csv-head --from $c --to $(( $c + $part )) &
    echo "/bin/time -f 'task ${i} %E' ./get_data.out --has-exchange-info --bfile ${i}.out --hfile ${i}h.out --csvfile ${i}.csv --no-csv-head --from $c --to $(( $c + $part )) &"
    c=$(( $c + $part ))
    i=$(( $i + 1 ))
done

wait
bash ./merge.sh -t ${tasks}
