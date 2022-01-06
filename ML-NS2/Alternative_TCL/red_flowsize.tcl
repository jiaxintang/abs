set N 100
set B 10000
set RTT 0.1;	# default 0.0001

set IP "127.0.0.1"
set PORT 10000

set lambda [expr [lindex $argv 1]];
set num_pkts [expr [lindex $argv 2]];
set SEEDS [expr [lindex $argv 3]];

#set sourceAlg DC-TCP-Sack
#set sourceAlg Newreno
set sourceAlg Cubic
set switchAlg RED
#set bgRate 0.5Gb
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
set recordFlowInfo 1;

set ns [new Simulator]

Agent/TCP set ecn_ 1
Agent/TCP set old_ecn_ 1
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

if {$recordFlowInfo != 0} {
	
	set flow_startingtime [open flow-startingtime_red.txt w]
	set recordall [open traceall-red.ta w]
	$ns trace-all $recordall
}


proc finish {} {
		global ns enableNAM namfile recordFlowInfo
		$ns flush-trace
		if {$enableNAM != 0} {
			close $namfile
			exec nam out.nam &
		}
		if {$recordFlowInfo !=0} {
			close $flow_startingtime;
			close $recordall;
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
	set n($i) [$ns node]
}

set node_bg [$ns node];

set nqueue [$ns node]
set nclient [$ns node]


$nqueue color red
$nqueue shape box
$nclient color blue

for {set i 0} {$i < $N} {incr i} {
	$ns duplex-link $n($i) $nqueue $inputLineRate [expr $RTT/4] DropTail
	$ns duplex-link-op $n($i) $nqueue queuePos 0.25
}

$ns duplex-link $node_bg $nqueue $inputLineRate [expr $RTT/4] DropTail
$ns duplex-link-op $node_bg $nqueue queuePos 0.25

$ns simplex-link $nqueue $nclient $bottleneckRate [expr $RTT/4] $switchAlg
$ns simplex-link $nclient $nqueue $bottleneckRate [expr $RTT/4] DropTail
$ns queue-limit $nqueue $nclient $B

$ns duplex-link-op $nqueue $nclient color "green"
$ns duplex-link-op $nqueue $nclient queuePos 0.25
#set qfile [$ns monitor-queue $nqueue $nclient [open queue.tr w] $traceSamplingInterval]

#------ traffic generation --------
set rng [new RNG]
$rng seed $SEEDS
set interval [new RandomVariable/Uniform];
$interval use-rng $rng;
$interval set min_ 0.0
$interval set max_ 1.0

set num_flow_ 0;

for {set i 0} {[expr $i*1.0/100]< $simulationTime} {incr i} {

	set cur_time [expr $i*1.0/100];
	set temp_loc [expr $num_flow_%$N];

	set sentdata [$interval value];
	if { $sentdata <= $lambda} {

		if {[string compare $sourceAlg "Newreno"] == 0 || [string compare $sourceAlg "DC-TCP-Newreno"] == 0} {
			set tcp($num_flow_) [new Agent/TCP/Newreno]
			set sink($num_flow_) [new Agent/TCPSink]
		}
		if {[string compare $sourceAlg "Sack"] == 0 || [string compare $sourceAlg "DC-TCP-Sack"] == 0} { 
			set tcp($num_flow_) [new Agent/TCP/FullTcp/Sack]
			set sink($num_flow_) [new Agent/TCP/FullTcp/Sack]
			$sink($num_flow_) listen
		}
		if {[string compare -nocase $sourceAlg "Cubic"] == 0} {

			set tcp($num_flow_) [new Agent/TCP/Linux];
			set sink($num_flow_) [new Agent/TCPSink/Sack1];
			$sink($num_flow_) set ts_echo_rfc1323_ true;
			$ns at 0 "$tcp($num_flow_) select_ca cubic"
		}
	
		if {[string compare -nocase $sourceAlg "Vegas"] == 0} {
			set tcp($num_flow_) [new Agent/TCP/Linux]
			set sink($num_flow_) [new Agent/TCPSink/Sack1]
			$ns at 0 "$tcp($num_flow_) select_ca vegas"
			$sink($num_flow_) set ts_echo_rfc1323_ true
			$tcp($num_flow_) set timestamps_ true
		}
		
		$ns attach-agent $n($temp_loc) $tcp($num_flow_)
		$ns attach-agent $nclient $sink($num_flow_)
		
		$tcp($num_flow_) set fid_ [expr $num_flow_]
		$sink($num_flow_) set fid_ [expr $num_flow_]
	
		$ns connect $tcp($num_flow_) $sink($num_flow_)

		set ftp($num_flow_) [new Application/FTP];
		$ftp($num_flow_) attach-agent $tcp($num_flow_);
		
		set st $cur_time;
		puts $flow_startingtime "$num_flow_ $st";
		$ns at $st "$ftp($num_flow_) produce $num_pkts";

		incr num_flow_;
	}
}

#set udp [new Agent/UDP];
#set null [new Agent/Null];
#$udp set fid_ [expr $num_flow_+1];
#$ns attach-agent $node_bg $udp;
#$ns attach-agent $nclient $null;
#$ns connect $udp $null;
#set cbr [new Application/Traffic/CBR];
#$cbr attach-agent $udp;
#$cbr set packetSize_ 1460;
#$cbr set rate_ $bgRate;
#$cbr set random_ false;
#$ns at 0.001 "$cbr start"
##$ns at [expr $bg_StopTime] "$cbr stop"

$tcp(0) setsocket $IP $PORT;
