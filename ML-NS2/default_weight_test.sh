#!/bin/bash
#!/bin/sh

MAX=100
NUM_CC=20
for (( i=1; i<$[$MAX]; i=i+$[$NUM_CC]))
{
	for (( j=0; j<$[$NUM_CC]; j++))
	{
		./ns-allinone-2.35/ns-2.35/ns enhanced_simplified_vrq_test.tcl 300 $[$i+$j]> $[$i+$j].out.txt &
	}
	wait
}

for (( i=1; i<=$[$MAX]; i++))
{
	./avg_thp_handler.sh $i
}

rm *.out.txt
