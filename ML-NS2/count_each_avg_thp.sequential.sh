#!/bin/sh
#!/bin/bash

START=${2}
END=${3}

for (( i=$[${START}]; i<=$[${END}]; i++ ))
{
	A=`cat ${1} | grep "thp@" | grep " ${i} " | awk 'NR==1{sum=$3;counter=0;next}{sum=sum+$3; counter=counter+1}END{print sum/(counter+1)}'`
	echo "${i}	${A}"
}
