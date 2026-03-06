TEMP=$(getopt -o ht: --long help,tasks: -n 'merge.sh' -- "$@")

if [ $? -ne 0 ]; then >&2 ; exit 1; fi

eval set -- "$TEMP"

usage(){
    echo "-h, --help"
    echo "      show this message"
    echo "-t, --tasks x"
    echo "      set number of tasks to merge default is 4"
}

tasks=4

while true; do
    case "$1" in
        -h | --help) usage ; exit ;;
        -t | --tasks) tasks=$2 ; shift 2 ;;
        --) shift ; break;;
        *) break;;
    esac
done

i=1

echo -n "coin_id,symbol,date,open,high,low,close,volume,market_cap" > historical_data.csv

while [ $i -le $tasks ]
do
 cat ${i}.csv >> historical_data.csv
 rm ${i}.out ${i}.csv
 i=$(( $i + 1 ))
done
