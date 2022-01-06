#!/bin/sh
#!/bin/bash

START=${1}
END=${2}

for (( i=$[${START}]; i<=$[${END}]; i++ ))
{
	A=`cat thp_B${i}.thp | awk 'NR==1{sum=$2;counter=0;next}{sum=sum+$2; counter=counter+1}END{print sum/(counter+1)}'`
	echo "${i}	${A}"
}
