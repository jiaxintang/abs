#!/bin/sh
#!/bin/bash

NUM_RTT=${2}
FLOWS_PER_RTT=${3}

for (( j=1; j<=$[${NUM_RTT}]; j++ ))
{
	for (( i=${j}; i<=$[${FLOWS_PER_RTT}*${NUM_RTT}]; i=$[${i}+${NUM_RTT}] ))
	{
		A=`cat ${1} | grep "thp@" | grep " ${i} " | awk 'NR==1{sum=$3;counter=0;next}{sum=sum+$3; counter=counter+1}END{print sum/(counter+1)}'`
		echo "${i}	${A}"
	}
}
