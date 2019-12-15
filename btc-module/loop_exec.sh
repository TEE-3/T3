#!/bin/bash
for (( c=0; c<$1; c++ ))
do  
   echo "************ RUN $c ************"
   sh ./exec.sh 
done
