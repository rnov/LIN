#!/bin/bash

for i in $(seq 1 210 );
	do 
		sleep 0.02
		echo add  $i > /proc/modlist;
		cat /proc/modlist;
done
