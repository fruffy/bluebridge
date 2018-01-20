#!/bin/bash


function linearize {
    INPUT=$1
    OUTPUT=$2
    cat $1 | tail -21 | head -20 > $1

    while IFS='' read -r line || [[ -n "$line" ]]; do
        echo -n "$line", >> $2
    done < "$1"
}

function run {
    INPUT=$1
    OUTPUT=$2
    TOTAL=$3
    LOCAL=$4
    MEM=$5
    ALG=$6
    PROG=$7
    GRAPH=$8
    #rm $OUTPUT
    echo TOTAL:$TOTAL LOCAL:$LOCAL ALG:$ALG PROG:$PROG GRAPH:$GRAPH >> $OUTPUT
    for i in `seq 1 1`;
    do
        ./applications/bin/rmem_test tmp/config/distMem.cnf $TOTAL $LOCAL $MEM $ALG $PROG $GRAPH >> $INPUT
        linearize $INPUT $OUTPUT
    done
    cat $OUTPUT
    echo "" >> $OUTPUT
    #mv $OUTPUT results
}

function clean {
    rm -r results
}

#clean
mkdir results
#run tmp.txt rmem.txt 1000 200 rmem lru pr wiki-Vote.txt
run tmp.txt rrmem.txt 1000 200 rrmem lru pr wiki-Vote.txt
#run tmp.txt rmem.txt 1000 100 rmem lru pr wiki-Vote.txt
run tmp.txt rrmem.txt 1000 100 rrmem lru pr wiki-Vote.txt
#run tmp.txt rmem.txt 1000 50 rmem lru pr wiki-Vote.txt
run tmp.txt rrmem.txt 1000 50 rrmem lru pr wiki-Vote.txt
#run tmp.txt rmem.txt 1000 25 rmem lru pr wiki-Vote.txt
run tmp.txt rrmem.txt 1000 25 rrmem lru pr wiki-Vote.txt
#run tmp.txt rmem.txt 1000 10 rmem lru pr wiki-Vote.txt
run tmp.txt rrmem.txt 1000 10 rrmem lru pr wiki-Vote.txt





