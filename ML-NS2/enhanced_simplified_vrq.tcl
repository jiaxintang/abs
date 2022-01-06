set N 40;
set B 10000
set RTT 0.0001;	# default 0.0001
puts "The base RTT between the switch and receiver is $RTT"

set IP "127.0.0.1"
set PORT 10000

set lineRate 1Gb;
set inputLineRate 10Gb;
set bottleneckRate 1Gb;
set simulationTime [expr [lindex $argv 0]];

set startMeasurementTime 1
set stopMeasurementTime 2
set flowClassifyTime 0.001

#set sourceAlg DC-TCP-Sack
set sourceAlg Newreno
set switchAlg SPriQueue
Queue/SPriQueue set num_queue_ 4;
Queue/SPriQueue set default_weight_ 2;
Queue/SPriQueue set rtt_lowerbound_ 0.00005;
Queue/SPriQueue set rtt_upperbound_ 0.01;
Queue/SPriQueue set weighted_rr_ 0;
Queue/SPriQueue set delay_based_dropping_ 1;
Queue/SPriQueue set avgbw_wrr_ 1;
Queue/SPriQueue set omag_queuediff_ 1;


set rtts(0) 0.00005;
set rtts(1) 0.001;
set rtts(2) 0.01;
set rtts(3) 0.2;

#set bgRate 0.05Gb
set bgRate 1Gb
set bg_StopTime [expr $simulationTime-0.2];	# debug only

set DCTCP_g_ 0.0625
set ackRatio 1 
set packetSize 1460
 
set Kmin_pkt 10;
set Kmax_pkt 30;
set Kmin [expr $Kmin_pkt*$packetSize];
set Kmax [expr $Kmax_pkt*$packetSize];
set wq 1.0;

set traceSamplingInterval 0.0001
set throughputSamplingInterval 0.002
set enableNAM 0
set enableTraceAll 0;

set ns [new Simulator]

Agent/TCP set ecn_ 0
Agent/TCP set old_ecn_ 0
Agent/TCP set packetSize_ $packetSize
Agent/TCP/FullTcp set segsize_ $packetSize
Agent/TCP set window_ 1256
Agent/TCP set slow_start_restart_ false
Agent/TCP set tcpTick_ 0.01
Agent/TCP set minrto_ 0.2 ; # minRTO = 200ms
Agent/TCP set windowOption_ 0


if {[string compare $sourceAlg "DC-TCP-Sack"] == 0} {
	Agent/TCP set dctcp_ true
	Agent/TCP set dctcp_g_ $DCTCP_g_;
}
Agent/TCP/FullTcp set segsperack_ $ackRatio; 
Agent/TCP/FullTcp set spa_thresh_ 3000;
Agent/TCP/FullTcp set interval_ 0.04 ; #delayed ACK interval = 40ms

Queue set limit_ 1000

Queue/RED set bytes_ false
Queue/RED set queue_in_bytes_ true
Queue/RED set mean_pktsize_ $packetSize
Queue/RED set setbit_ false
Queue/RED set gentle_ false
Queue/RED set q_weight_ $wq
Queue/RED set mark_p_ 1.0
#Queue/RED set use_mark_p_ false

Queue/RED set thresh_queue_ $Kmin
Queue/RED set maxthresh_queue_ $Kmax
Queue/RED set thresh_ [expr $Kmin_pkt]
Queue/RED set maxthresh_ [expr $Kmax_pkt]

Queue/RED set init_interval_ 1;

DelayLink set avoidReordering_ true

if {$enableNAM != 0} {
	set namfile [open out.nam w]
	$ns namtrace-all $namfile
}

if {$enableTraceAll != 0} {
	set tracefile [open traceall.ta w]
	$ns trace-all $tracefile
}


proc finish {} {
		global ns enableNAM namfile mytracefile throughputfile
		$ns flush-trace
		close $mytracefile
		close $throughputfile
		if {$enableNAM != 0} {
		close $namfile
		exec nam out.nam &
	}
	exit 0
}

$ns color 0 Red
$ns color 1 Orange
$ns color 2 Yellow
$ns color 3 Green
$ns color 4 Blue
$ns color 5 Violet
$ns color 6 Brown
$ns color 7 Black

for {set i 0} {$i < $N} {incr i} {
	set s($i) [$ns node]
	set r($i) [$ns node]
}

#set node_bg [$ns node];

set s_tor [$ns node]
set r_tor [$ns node]

#set mid_node [$ns node];

$s_tor color red
$s_tor shape box
$r_tor color blue
$r_tor shape box


for {set i 0} {$i < $N} {incr i} {
	
#	set cur_rtt [expr $rtts($i)/4];
	set cur_rtt [expr $rtts([expr $i/10])/4];
	puts "cur_rtt@node($i):[expr $cur_rtt*4+$RTT]";
	set rtt_node($i) [expr $cur_rtt*4+$RTT];
	$ns duplex-link $s($i) $s_tor $inputLineRate [expr $cur_rtt] DropTail
	$ns duplex-link-op $s($i) $s_tor queuePos 0.25

	$ns duplex-link $r($i) $r_tor $lineRate [expr $cur_rtt] DropTail
	$ns duplex-link-op $r($i) $r_tor queuePos 0.25
}

$ns simplex-link $s_tor $r_tor $bottleneckRate [expr $RTT/2] $switchAlg
$ns simplex-link $r_tor $s_tor $bottleneckRate [expr $RTT/2] DropTail
$ns queue-limit $s_tor $r_tor $B;
$ns queue-limit $r_tor $s_tor $B;

$ns duplex-link-op $s_tor $r_tor color "green"
$ns duplex-link-op $s_tor $r_tor queuePos 0.25


for {set i 0} { [expr $i] < $N } { incr i } {

	if {[string compare $sourceAlg "Newreno"] == 0 || [string compare $sourceAlg "DC-TCP-Newreno"] == 0} {
		set tcp($i) [new Agent/TCP/Newreno]
		set sink($i) [new Agent/TCPSink]
	}
	if {[string compare $sourceAlg "Sack"] == 0 || [string compare $sourceAlg "DC-TCP-Sack"] == 0} { 
		set tcp($i) [new Agent/TCP/FullTcp/Sack]
		set sink($i) [new Agent/TCP/FullTcp/Sack]
		$sink($i) listen
	}

	$ns attach-agent $s($i) $tcp($i)
	$ns attach-agent $r($i) $sink($i)
	
	$tcp($i) set fid_ [expr $i+1]
	$sink($i) set fid_ [expr $i+1]

	$ns connect $tcp($i) $sink($i);

	set ftp($i) [new Application/FTP];
        $ftp($i) attach-agent $tcp($i);

	$ns at 0 "$ftp($i) start"
	$ns at $simulationTime "$ftp($i) stop";
}

$tcp(0) setsocket $IP $PORT;
$ns at $simulationTime "finish"

$defaultRNG seed 0
$ns run
