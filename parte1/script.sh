#!/bin/bash

end=$((SECONDS+3))

while [ $SECONDS -lt $end ]; do
	for i in $(seq 1 150 );
	do 
		echo add  $i > /proc/modlist;
		echo remove $i > /proc/modlist;
		cat /proc/modlist;
	done
done
