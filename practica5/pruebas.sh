#!/bin/bash

echo create list0 > /proc/multilist/control
echo create list1 > /proc/multilist/control

ls /proc/multilist/

echo add 1 > /proc/multilist/default
echo add 2 > /proc/multilist/list0
echo add 3 > /proc/multilist/list1

echo add 4 > /proc/multilist/default
echo add 5 > /proc/multilist/list0
echo add 6 > /proc/multilist/list1

echo add 7 > /proc/multilist/default
echo add 8 > /proc/multilist/list0
echo add 9 > /proc/multilist/list1

echo entry default :
cat /proc/multilist/default
echo entry list0 :
cat /proc/multilist/list0
echo entry list1 :
cat /proc/multilist/list1




