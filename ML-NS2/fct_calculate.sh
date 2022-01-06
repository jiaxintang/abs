#!/bin/bash
#!/bin/sh

if [ 1 -eq $# ]; then

	fstimefile="flow-startingtime_${1}.txt"
	tracefile="traceall-${1}.ta"
	NUM_FLOWS=`tail -n 1 ${fstimefile} | awk 'NR==1{print $1}'`
	echo "total # of flows: "${NUM_FLOWS}
	
	for (( i=0; i<$[${NUM_FLOWS}]; i++ ))
	{
	
		tfname="temp_${i}-${1}.txt"
		cat ${tracefile} | grep '\- '${i}' ' > ${tfname}
		endtime=`tail -n 1 ${tfname} | awk 'NR==1{print $2}'`
	#	echo "end time of flow ${i} is "${endtime}
		line_num="${i}p"
		starttime=`sed -n ${line_num} ${fstimefile} | awk 'NR==1{print $2}'`
	#	echo "start time of flow ${i} is "${starttime}
		duration=`echo ${endtime}" "${starttime} | awk '{printf("%0.3f\n", $1-$2)}'`
		echo ${duration}
		rm ${tfname}
	}

else
	echo "Usage: ./fct_calculate.sh <type>"
	echo "	<type>: codel"
	echo "		red"
fi
