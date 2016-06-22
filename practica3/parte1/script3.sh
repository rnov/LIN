#!/bin/bash

for i in $(seq 1 200 );
	do 
		sleep 0.02
		echo remove  $i > /proc/modlist;
done
