#!/bin/sh
#!/bin/bash

echo "Please select one trace:"
echo "	1. (RED) ATT-LTE-driving-2016.down"
echo "	2. (RED) ATT-LTE-driving.down"
echo "	3. (RED) TMobile-LTE-driving.down"
echo "	4. (RED) TMobile-LTE-short.down"
echo "	5. (RED) TMobile-UMTS-driving.down"
echo "	6. (RED) Verizon-EVDO-driving.dow"
echo "	7. (RED) Verizon-LTE-driving.down"
echo "	8. (RED) Verizon-LTE-short.down"
echo "	9. (CoDel) ATT-LTE-driving-2016.down"
echo "	10.(CoDel) ATT-LTE-driving.down"
echo "	11.(CoDel) TMobile-LTE-driving.down"
echo "	12.(CoDel) TMobile-LTE-short.down"
echo "	13.(CoDel) TMobile-UMTS-driving.down"
echo "	14.(CoDel) Verizon-EVDO-driving.dow"
echo "	15.(CoDel) Verizon-LTE-driving.down"
echo "	16.(CoDel) Verizon-LTE-short.down"

read -p "" opt

echo "Your Selection is ${opt}"

echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
read -p "" fa
echo "please enter the size of flows (in # of packets):"
read -p "" fs
echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
read -p "" seed

if [ 1 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving-2016.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 2 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 3 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns TMobile-LTE-driving.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 4 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 5 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns TMobile-UMTS-driving.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 6 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns Verizon-EVDO-driving.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 7 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 8 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns Verizon-LTE-short.down.tcl 0 ${fa} ${fs} ${seed}
elif [ 9 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving-2016.down.tcl 1 ${fa} ${fs} ${seed}
elif [ 10 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving.down.tcl 1 ${fa} ${fs} ${seed}
elif [ 11 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-LTE-driving.down.tcl 1 ${fa} ${fs} ${seed}
elif [ 12 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl 1 ${fa} ${fs} ${seed}
elif [ 13 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-UMTS-driving.down.tcl 1 ${fa} ${fs} ${seed}
elif [ 14 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-EVDO-driving.down.tcl 1 ${fa} ${fs} ${seed}
elif [ 15 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl 1 ${fa} ${fs} ${seed}
elif [ 16 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-LTE-short.down.tcl 1 ${fa} ${fs} ${seed}
else
	echo "Wrong input!"
fi
