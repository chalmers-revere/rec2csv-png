#!/bin/bash

FPS=5

for d in $(ls); do
    if [ -d $d ]; then
        cd $d
        CNT=0
        FIRST=0
        SECOND=0
        for i in $(ls *png); do
            if [ "$CNT" == "0" ]; then
                FIRST=$(echo $i | cut -f1 -d".")
            fi
            if [ "$CNT" == "1" ]; then
                SECOND=$(echo $i | cut -f1 -d".")
                FPS=$(echo "1000/(($SECOND-$FIRST)/1000)"|bc)
            fi
            printf -v j "%09d" $CNT
            CNT=$((CNT + 1))
            mv $i $j.png
        done

        if [ "$FPS" == "" ]; then
            FPS=10
        fi

        echo "Using $FPS fps."

        png2yuv -I p -f $FPS -j %09d.png -n $(ls -l *png|wc -l) > clip.yuv

        vpxenc --good --cpu-used=0 --auto-alt-ref=1 --lag-in-frames=16 --end-usage=vbr --passes=2 --threads=2 --target-bitrate=3000 -o ../$d.webm clip.yuv
        cd ..
    fi
done
