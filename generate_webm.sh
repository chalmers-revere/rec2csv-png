#!/bin/bash

# Copyright (C) 2018  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

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

        vpxenc --good --cpu-used=2 --auto-alt-ref=1 --lag-in-frames=16 --end-usage=vbr --passes=2 --threads=4 --target-bitrate=3000 --minsection-pct=15 --maxsection-pct=400 --kf-min-dist=0 --kf-max-dist=360 --static-thresh=0 --min-q=4 --max-q=63 -o ../$d.webm clip.yuv
        cd ..
    fi
done
