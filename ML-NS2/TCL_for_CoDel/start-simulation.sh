#!/bin/sh
#!/bin/bash

echo "Please select one trace:"
echo "	1.(CoDel) TMobile-LTE-short.down"
echo "	2.(CoDel, TCP variants, custom flow size) TMobile-LTE-short.down"
echo "	3.(CoDel) Verizon-LTE-driving.down"
echo "	4.(CoDel, TCP variants, custom flow size) Verizon-LTE-driving.down"
echo "	5.Regular Bandwidth variation (CoDel, Cubic, Sine signal, 2x frequence, 2x bandwidth capacity)"
echo "	6.Desynchronized pattern with fixed seeds (Cubic, CoDel, RTT 10x larger & custom simulation duration)"
echo "	7.Desynchronized pattern with fixed seeds (TCP variants, dynamic capacity, RTT 10x larger & custom simulation duration)"

read -p "" opt

echo "Your Selection is ${opt}"


echo "Please enter the measurement interval for CoDel (in Seconds):"
read -p "" itv
echo "Please enter the target delay for CoDel (in Seconds):"
read -p "" tg


if [ 1 -eq ${opt} ]; then

	../CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl Newreno ${itv} ${tg}

elif [ 2 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average flow size (i.e., number of packets to send):"
	read -p "" fs
	echo "please enter the value of seed ( 0 means completely random):"
	read -p "" seed
	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl ${fa} ${fs} ${seed} ${tcp} ${itv} ${tg}

elif [ 3 -eq ${opt} ]; then

	../CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl Newreno ${itv} ${tg}

elif [ 4 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average flow size (i.e., number of packets to send):"
	read -p "" fs
	echo "please enter the value of seed ( 0 means completely random):"
	read -p "" seed
	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl ${fa} ${fs} ${seed} ${tcp} ${itv} ${tg}

elif [ 5 -eq ${opt} ]; then

	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../CoDel/ns-allinone-2.35/ns-2.35/ns regular_bw_variation_morepeak.tcl ${tcp} ${itv} ${tg} 
elif [ 6 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../CoDel/ns-allinone-2.35/ns-2.35/ns dcn-desynchronization_RTTEnlarge_CustomDuration.codel.tcl ${fa} ${fttl} ${seed} ${duration} ${tcp} ${itv} ${tg}
elif [ 7 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../CoDel/ns-allinone-2.35/ns-2.35/ns desynchronization_RTTEnlarge_dynamic_capacity_CustomDuration.codel.tcl ${fa} ${fttl} ${seed} ${duration} ${tcp} ${itv} ${tg}

else
	echo "Wrong input!"
fi
