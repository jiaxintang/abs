#!/bin/sh
#!/bin/bash

echo "Please select one trace:"
echo "	1.Desynchronized pattern with VRQ (fixed seeds, 10x RTT, and custom simulation duration)"
echo "	2.Desynchronized pattern with VRQ (fixed seeds, 10x RTT, and custom flowsize & simulation duration)"
echo "	3.Synchronized pattern with VRQ (10x RTT and custom simulation duration)"
echo "	4.Enhanced VRQ (delay-based dropping, average-queueing-delay-based weighing, order-of-magnitude enqueueing)"
echo "	5.Enhanced VRQ with multiple TCP variants and multiple flows for certain RTT range (delay-based dropping, average-queueing-delay-based weighing, order-of-magnitude enqueueing)"
echo "	6.Enhanced VRQ with multiple TCP variants and multiple flows for certain RTT range (delay-based dropping, average-queueing-delay-based weighing, order-of-magnitude enqueueing, customized number of flows in each RTT)"

read -p "" opt

echo "Your Selection is ${opt}"

if [ 1 -eq ${opt} ]; then
	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	echo "use weighted round-robin? (0:no, 1:yes)"
	read -p "" iswrr
	./ns-allinone-2.35/ns-2.35/ns vrq.tcl ${fa} ${fttl} ${seed} ${duration} ${iswrr}

elif [ 2 -eq ${opt} ]; then
	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average flow size (i.e., number of packets to send):"
	read -p "" fs
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	echo "use weighted round-robin? (0:no, 1:yes)"
	read -p "" iswrr
	./ns-allinone-2.35/ns-2.35/ns vrq_fixflowsize.tcl ${fa} ${fs} ${seed} ${duration} ${iswrr}

elif [ 3 -eq ${opt} ]; then
	echo "please enter the simulation time:"
	read -p "" duration
	./ns-allinone-2.35/ns-2.35/ns simplified_vrq.tcl ${duration}

elif [ 4 -eq ${opt} ]; then
	echo "please enter the simulation time:"
	read -p "" duration
	./ns-allinone-2.35/ns-2.35/ns enhanced_simplified_vrq.tcl ${duration}

elif [ 5 -eq ${opt} ]; then
	echo "please enter the simulation time:"
	read -p "" duration
	echo "please choose TCP:"
	echo "	Newreno"
	echo "	Cubic"
	echo "	Vegas"
	read -p "" tcp
	./ns-allinone-2.35/ns-2.35/ns enhanced_simplified_vrq_tcpvariant.tcl ${duration} ${tcp}
elif [ 6 -eq ${opt} ]; then
	echo "please enter the simulation time:"
	read -p "" duration
	echo "please choose TCP:"
	echo "	Newreno"
	echo "	Cubic"
	echo "	Vegas"
	read -p "" tcp
	./ns-allinone-2.35/ns-2.35/ns enhanced_simplified_vrq_tcpvariant.diffRTT.tcl ${duration} ${tcp}

else
	echo "Wrong input!"
fi
