#!/bin/bash
#!/bin/sh
BASE=${1}
cat ${1}.out.txt | grep "queue#0" > ${1}.queue0.txt 
cat ${1}.out.txt | grep "queue#1" > ${1}.queue1.txt 
cat ${1}.out.txt | grep "queue#2" > ${1}.queue2.txt 
cat ${1}.out.txt | grep "queue#3" > ${1}.queue3.txt 

awk -F " " '{print $10}' ${1}.queue0.txt >${1}.temp_que0.txt
awk -F " " '{print $10}' ${1}.queue1.txt >${1}.temp_que1.txt
awk -F " " '{print $10}' ${1}.queue2.txt >${1}.temp_que2.txt
awk -F " " '{print $10}' ${1}.queue3.txt >${1}.temp_que3.txt

cat ${1}.temp_que0.txt | tr -d 'thrp('  > ${1}.queue0.txt
cat ${1}.temp_que1.txt | tr -d 'thrp('  > ${1}.queue1.txt
cat ${1}.temp_que2.txt | tr -d 'thrp('  > ${1}.queue2.txt
cat ${1}.temp_que3.txt | tr -d 'thrp('  > ${1}.queue3.txt

cat ${1}.queue0.txt | tr -d ')'  > ${1}.temp_que0.txt
cat ${1}.queue1.txt | tr -d ')'  > ${1}.temp_que1.txt
cat ${1}.queue2.txt | tr -d ')'  > ${1}.temp_que2.txt
cat ${1}.queue3.txt | tr -d ')'  > ${1}.temp_que3.txt

paste ${1}.temp_que0.txt ${1}.temp_que1.txt ${1}.temp_que2.txt ${1}.temp_que3.txt > ${1}.final.txt
rm ${1}.queue*.txt
rm ${1}.temp_que*.txt
echo -n "${1}	:"
cat ${1}.final.txt | awk '{sum_0+=$1; sum_1+=$2; sum_2+=$3; sum_3+=$4;}END{print "", sum_0/NR, sum_1/NR, sum_2/NR, sum_3/NR}'
rm ${1}.final.txt
