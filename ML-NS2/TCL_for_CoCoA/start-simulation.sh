#!/bin/sh
#!/bin/bash

echo "Please select one trace:"
echo "	1.(CoCoA) TMobile-LTE-short.down"
echo "	2.(CoCoA, TCP variants, custom flow size) TMobile-LTE-short.down"
echo "	3.(CoCoA) Verizon-LTE-driving.down"
echo "	4.(CoCoA, TCP variants, custom flow size) Verizon-LTE-driving.down"
echo "	5.Regular Bandwidth variation (CoCoA, Cubic, Sine signal, 2x frequence, 2x bandwidth capacity)"
echo "	6.Desynchronized pattern with fixed seeds (Cubic, CoCoA, RTT 10x larger & custom simulation duration)"
echo "	7.Desynchronized pattern with fixed seeds (TCP variants, CoCoA, dynamic capacity, RTT 10x larger & custom simulation duration)"

read -p "" opt

echo "Your Selection is ${opt}"


echo "Please enter the LI for CoCoA (in Seconds):"
read -p "" li


if [ 1 -eq ${opt} ]; then

	../ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl Newreno ${li}

elif [ 2 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average flow size (i.e., number of packets to send):"
	read -p "" fs
	echo "please enter the value of seed ( 0 means completely random):"
	read -p "" seed
	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl ${fa} ${fs} ${seed} ${tcp} ${li}

elif [ 3 -eq ${opt} ]; then

	../ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl Newreno ${li}

elif [ 4 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average flow size (i.e., number of packets to send):"
	read -p "" fs
	echo "please enter the value of seed ( 0 means completely random):"
	read -p "" seed
	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl ${fa} ${fs} ${seed} ${tcp} ${li}

elif [ 5 -eq ${opt} ]; then

	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	../ns-allinone-2.35/ns-2.35/ns regular_bw_variation_morepeak.tcl ${tcp} ${li} 
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
	../ns-allinone-2.35/ns-2.35/ns dcn-desynchronization_RTTEnlarge_CustomDuration.cocoa.tcl ${fa} ${fttl} ${seed} ${duration} ${tcp} ${li}
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
	../ns-allinone-2.35/ns-2.35/ns desynchronization_RTTEnlarge_dynamic_capacity_CustomDuration.cocoa.tcl ${fa} ${fttl} ${seed} ${duration} ${tcp} ${li}
else
	echo "Wrong input!"
fi
