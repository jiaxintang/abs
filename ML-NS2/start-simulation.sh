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
echo "	9. Incast pattern in Data Center Network"
echo "	10.Incast pattern in Data Center Network (smooth increasing)"
echo "	11.Incast pattern in Data Center Network (fix # of senders)"
echo "	12.Incast pattern in Data Center Network (fixed RTT)"
echo "	13.Incast pattern in Data Center Network (varying RTT)"
echo "	14.Desynchronized pattern (completely random)"
echo "	15.Desynchronized pattern (with fixed seeds)"
echo "	16.Desynchronized pattern with fixed seeds (RTT 10x larger)"
echo "	17.Desynchronized pattern with fixed seeds (RTT 10x larger & custom simulation duration)"
echo "	18.Regular Bandwidth variation (Sine signal)"
echo "	19.Regular Bandwidth variation (Sine signal, 2x frequence, 2x bandwidth capacity)"
echo "	20.Desynchronized pattern with VRQ (fixed seeds, 10x RTT, and custom simulation duration)"
echo "	21.Desynchronized pattern with VRQ (fixed seeds, 10x RTT, and custom flowsize & simulation duration)"
echo "	22.Synchronized pattern with VRQ (10x RTT and custom simulation duration)"
echo "	23.Enhanced VRQ (delay-based dropping, average-queueing-delay-based weighing, order-of-magnitude enqueueing)"
echo "	24.Enhanced VRQ with multiple TCP variants and multiple flows for certain RTT range (delay-based dropping, average-queueing-delay-based weighing, order-of-magnitude enqueueing)"
echo "	25.Regular Bandwidth variation (CoDel, Sine signal, 2x frequence, 2x bandwidth capacity)"
echo "	26.(CoDel) ATT-LTE-driving-2016.down"
echo "	27.(CoDel) ATT-LTE-driving.down"
echo "	28.(CoDel) TMobile-LTE-driving.down"
echo "	29.(CoDel) TMobile-LTE-short.down"
echo "	30.(CoDel) TMobile-UMTS-driving.down"
echo "	31.(CoDel) Verizon-EVDO-driving.dow"
echo "	32.(CoDel) Verizon-LTE-driving.down"
echo "	33.(CoDel) Verizon-LTE-short.down"
echo "	34.Regular Bandwidth variation (Cubic, Sine signal, 2x frequence, 2x bandwidth capacity)"
echo "	35.Regular Bandwidth variation (CoDel, Cubic, Sine signal, 2x frequence, 2x bandwidth capacity)"
echo "	36.Desynchronized pattern with fixed seeds (Cubic, RTT 10x larger & custom simulation duration)"
echo "	37.Desynchronized pattern with fixed seeds (Cubic, CoDel, RTT 10x larger & custom simulation duration)"
echo "	38.Random Bandwidth variation (Multiple TCP variants, Sine signal, 2x frequence, 2x bandwidth capacity)"
echo "	39.Desynchronized pattern with fixed seeds (TCP variants, dynamic capacity, RTT 10x larger & custom simulation duration)"

read -p "" opt

echo "Your Selection is ${opt}"

if [ 1 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving-2016.down.tcl
elif [ 2 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving.down.tcl
elif [ 3 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns TMobile-LTE-driving.down.tcl
elif [ 4 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl
elif [ 5 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns TMobile-UMTS-driving.down.tcl
elif [ 6 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns Verizon-EVDO-driving.down.tcl
elif [ 7 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl
elif [ 8 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns Verizon-LTE-short.down.tcl
elif [ 9 -eq ${opt} ]; then
	
	./ns-allinone-2.35/ns-2.35/ns dcn.tcl
elif [ 10 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns dcn-smooth.tcl
elif [ 11 -eq ${opt} ]; then

	echo "please enter the # of senders:"
	read -p "" numsd
	./ns-allinone-2.35/ns-2.35/ns dcn-smooth-fix.tcl ${numsd}
elif [ 12 -eq ${opt} ]; then

	echo "please enter the # of senders:"
	read -p "" numsd
	echo "please enter the ratio of RTT enlarged (e.g., to achieve '100us->100ms', you need to input 1000):"
	read -p "" rttd
	./ns-allinone-2.35/ns-2.35/ns dcn-fix_rtt.tcl ${numsd} ${rttd}
elif [ 13 -eq ${opt} ]; then
	
	echo "please enter the # of senders:"
	read -p "" numsd
	./ns-allinone-2.35/ns-2.35/ns dcn-rtt_hops.tcl ${numsd}
elif [ 14 -eq ${opt} ]; then
	
	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	./ns-allinone-2.35/ns-2.35/ns dcn-desynchronization.tcl ${fa} ${fttl} 0
elif [ 15 -eq ${opt} ]; then
	
	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	./ns-allinone-2.35/ns-2.35/ns dcn-desynchronization.tcl ${fa} ${fttl} ${seed}

elif [ 16 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	./ns-allinone-2.35/ns-2.35/ns dcn-desynchronization_RTTEnlarge.tcl ${fa} ${fttl} ${seed}

elif [ 17 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	./ns-allinone-2.35/ns-2.35/ns dcn-desynchronization_RTTEnlarge_CustomDuration.tcl ${fa} ${fttl} ${seed} ${duration}

elif [ 18 -eq ${opt} ]; then
	
	./ns-allinone-2.35/ns-2.35/ns regular_bw_variation.tcl
#	echo ""
#	echo ""
#	echo ""
#	echo ""
#	echo -n "start analysis the timeout in 3 "
#	sleep 1
#	echo -n "2 "
#	sleep 1
#	echo "1 "
#	sleep 1
#	echo "starting"
#	echo ""
#	echo ""
#	echo ""
#	echo ""
#
#	./timeout_analyzer.sh
#	rm traceall.tr

elif [ 19 -eq ${opt} ]; then
	./ns-allinone-2.35/ns-2.35/ns regular_bw_variation_morepeak.tcl Newreno

elif [ 20 -eq ${opt} ]; then
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

elif [ 21 -eq ${opt} ]; then
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

elif [ 22 -eq ${opt} ]; then
	echo "please enter the simulation time:"
	read -p "" duration
	./ns-allinone-2.35/ns-2.35/ns simplified_vrq.tcl ${duration}

elif [ 23 -eq ${opt} ]; then
	echo "please enter the simulation time:"
	read -p "" duration
	./ns-allinone-2.35/ns-2.35/ns enhanced_simplified_vrq.tcl ${duration}

elif [ 24 -eq ${opt} ]; then
	echo "please enter the simulation time:"
	read -p "" duration
	echo "please choose TCP:"
	echo "	Newreno"
	echo "	Cubic"
	echo "	Vegas"
	read -p "" tcp
	./ns-allinone-2.35/ns-2.35/ns enhanced_simplified_vrq_tcpvariant.tcl ${duration} ${tcp}
elif [ 25 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns regular_bw_variation_morepeak.tcl 0 Newreno
elif [ 26 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving-2016.down.tcl 0
elif [ 27 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns ATT-LTE-driving.down.tcl 0
elif [ 28 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-LTE-driving.down.tcl 0
elif [ 29 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-LTE-short.down.tcl 0
elif [ 30 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns TMobile-UMTS-driving.down.tcl 0
elif [ 31 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-EVDO-driving.down.tcl 0
elif [ 32 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-LTE-driving.down.tcl 0
elif [ 33 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns Verizon-LTE-short.down.tcl 0
elif [ 34 -eq ${opt} ]; then

	./ns-allinone-2.35/ns-2.35/ns regular_bw_variation_morepeak.tcl Cubic
elif [ 35 -eq ${opt} ]; then

	./CoDel/ns-allinone-2.35/ns-2.35/ns regular_bw_variation_morepeak.tcl 0 Cubic
elif [ 36 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	./ns-allinone-2.35/ns-2.35/ns dcn-desynchronization_RTTEnlarge_CustomDuration.tcl ${fa} ${fttl} ${seed} ${duration} Cubic
elif [ 37 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	./CoDel/ns-allinone-2.35/ns-2.35/ns dcn-desynchronization_RTTEnlarge_CustomDuration.codel.tcl ${fa} ${fttl} ${seed} ${duration} Cubic
elif [ 38 -eq ${opt} ]; then
	echo "please choose one AQM scheme (1. RED; 2. CoDel):"
	read -p "" aqm
	echo "please choose the TCP variant (Cubic, Newreno, Vegas, and etc., case sensitive):"
	read -p "" tcp
	echo "please enter the value of seed ( 0 means completely random):"
	read -p "" seed
	if [ 1 -eq ${aqm} ]; then
		./ns-allinone-2.35/ns-2.35/ns random_bw_variation.tcl ${tcp} ${seed}
	else
		./CoDel/ns-allinone-2.35/ns-2.35/ns random_bw_variation.tcl 0 ${tcp} ${seed}
	fi
elif [ 39 -eq ${opt} ]; then

	echo "please enter the average flow arrival ratio in every timeslot(i.e., PARAMETER*100=average # of flows in one second):"
	read -p "" fa
	echo "please enter the average transmission duration of flows:"
	read -p "" fttl
	echo "please enter the value of seed ( 0 means completely random, which is the same as option 14):"
	read -p "" seed
	echo "please enter simulation time:"
	read -p "" duration
	./ns-allinone-2.35/ns-2.35/ns desynchronization_RTTEnlarge_dynamic_capacity_CustomDuration.tcl ${fa} ${fttl} ${seed} ${duration} Cubic


else
	echo "Wrong input!"
fi
