set N 50
set B 10000
set RTT 0.1;	# default 0.0001

set IP "127.0.0.1"
set PORT 10001


set startMeasurementTime 1
set stopMeasurementTime 2
set flowClassifyTime 0.001

#set sourceAlg DC-TCP-Sack
#set sourceAlg Newreno
#set sourceAlg Cubic

if { [string compare [lindex $argv 0] "Cubic"] == 0 } {

	set sourceAlg Cubic;
} elseif { [string compare [lindex $argv 0] "Vegas"] == 0 } {

	set sourceAlg Vegas;
} else {
	
	set sourceAlg Newreno
}

set switchAlg CoCoA
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

## CoCoA values
Queue/CoCoA set init_interval_ 1;
Queue/CoCoA set li_min_thresh_ [expr [lindex $argv 1]]; 

DelayLink set avoidReordering_ true

if {$enableNAM != 0} {
	set namfile [open out.nam w]
	$ns namtrace-all $namfile
}

#set mytracefile [open cwnd_drop_[lindex $argv 0]-[lindex $argv 1].tr w]
#set throughputfile [open thrfile_[lindex $argv 0]-[lindex $argv 1].tr w]

proc finish {} {
		global ns enableNAM namfile mytracefile throughputfile
		$ns flush-trace
		if {$enableNAM != 0} {
		close $namfile
		exec nam out.nam &
	}
	exit 0
}

proc myTrace {file} {
	global ns N traceSamplingInterval tcp qfile MainLink nbow nclient packetSize enableBumpOnWire
	
	set now [$ns now]
	
	for {set i 0} {$i < $N} {incr i} {
		set cwnd($i) [$tcp($i) set cwnd_]
	}
	
	$qfile instvar parrivals_ pdepartures_ pdrops_ bdepartures_
  
	puts -nonewline $file "$now $cwnd(0)"
	for {set i 1} {$i < $N} {incr i} {
		puts -nonewline $file " $cwnd($i)"
	}
 
	puts -nonewline $file " [expr $parrivals_-$pdepartures_-$pdrops_]"	
	puts $file " $pdrops_"
	 
	$ns at [expr $now+$traceSamplingInterval] "myTrace $file"
}

proc throughputTrace {file} {
	global ns throughputSamplingInterval qfile flowstats N flowClassifyTime
	
	set now [$ns now]
	
	$qfile instvar bdepartures_
	
	puts -nonewline $file "$now [expr $bdepartures_*8/$throughputSamplingInterval/1000000]"
	set bdepartures_ 0
	if {$now <= $flowClassifyTime} {
	for {set i 0} {$i < [expr $N-1]} {incr i} {
		puts -nonewline $file " 0"
	}
	puts $file " 0"
	}

	if {$now > $flowClassifyTime} { 
	for {set i 0} {$i < [expr $N-1]} {incr i} {
		$flowstats($i) instvar barrivals_
		puts -nonewline $file " [expr $barrivals_*8/$throughputSamplingInterval/1000000]"
		set barrivals_ 0
	}
	$flowstats([expr $N-1]) instvar barrivals_
	puts $file " [expr $barrivals_*8/$throughputSamplingInterval/1000000]"
	set barrivals_ 0
	}
	$ns at [expr $now+$throughputSamplingInterval] "throughputTrace $file"
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


for {set i 0} {$i < $N} {incr i} {
	if {[string compare $sourceAlg "Newreno"] == 0 || [string compare $sourceAlg "DC-TCP-Newreno"] == 0} {
		set tcp($i) [new Agent/TCP/Newreno]
		set sink($i) [new Agent/TCPSink]
	}
	if {[string compare $sourceAlg "Sack"] == 0 || [string compare $sourceAlg "DC-TCP-Sack"] == 0} { 
		set tcp($i) [new Agent/TCP/FullTcp/Sack]
		set sink($i) [new Agent/TCP/FullTcp/Sack]
		$sink($i) listen
	}

	if {[string compare -nocase $sourceAlg "Cubic"] == 0} {

		set tcp($i) [new Agent/TCP/Linux];
		set sink($i) [new Agent/TCPSink/Sack1];
		$sink($i) set ts_echo_rfc1323_ true;
		$ns at 0 "$tcp($i) select_ca cubic"
	}

	if {[string compare -nocase $sourceAlg "Vegas"] == 0} {
		set tcp($i) [new Agent/TCP/Linux]
		set sink($i) [new Agent/TCPSink/Sack1]
		$ns at 0 "$tcp($i) select_ca vegas"
		$sink($i) set ts_echo_rfc1323_ true
		$tcp($i) set timestamps_ true
	}


	$ns attach-agent $n($i) $tcp($i)
	$ns attach-agent $nclient $sink($i)
	
	$tcp($i) set fid_ [expr $i]
	$sink($i) set fid_ [expr $i]

	$ns connect $tcp($i) $sink($i)	   
}

for {set i 0} {$i < $N} {incr i} {
	set ftp($i) [new Application/FTP]
	$ftp($i) attach-agent $tcp($i)	
}

set udp [new Agent/UDP];
set null [new Agent/Null];
$udp set fid_ [expr $N];
$ns attach-agent $node_bg $udp;
$ns attach-agent $nclient $null;
$ns connect $udp $null;
set cbr [new Application/Traffic/CBR];
$cbr attach-agent $udp;
$cbr set packetSize_ 1460;
$cbr set rate_ $bgRate;
$cbr set random_ false;
$ns at 0.001 "$cbr start"
#$ns at [expr $bg_StopTime] "$cbr stop"

$tcp(0) setsocket $IP $PORT;

#$ns at $traceSamplingInterval "myTrace $mytracefile"
#$ns at $throughputSamplingInterval "throughputTrace $throughputfile"

set ru [new RandomVariable/Uniform]
$ru set min_ 0
$ru set max_ 1.0

for {set i 0} {$i < $N} {incr i} {
	$ns at 0.0 "$ftp($i) send 1"

	$ns at [expr 0.001] "$ftp($i) start"
	#$ns at [expr 0.1 + $simulationTime * $i / ($N + 0.0001)] "$ftp($i) start"	 
	$ns at [expr $simulationTime] "$ftp($i) stop"
}

#set flowmon [$ns makeflowmon Fid]
#set MainLink [$ns link $nqueue $nclient]

#$ns attach-fmon $MainLink $flowmon

#set fcl [$flowmon classifier]

#$ns at $flowClassifyTime "classifyFlows"

proc classifyFlows {} {
	global N fcl flowstats
	puts "NOW CLASSIFYING FLOWS"
	for {set i 0} {$i < $N} {incr i} {
	puts "#.$i classified"
	set flowstats($i) [$fcl lookup autp 0 0 $i]
	}
} 


set startPacketCount 0
set stopPacketCount 0

proc startMeasurement {} {
	global qfile startPacketCount
	$qfile instvar pdepartures_   
	set startPacketCount $pdepartures_
}

proc stopMeasurement {} {
	global qfile startPacketCount stopPacketCount packetSize startMeasurementTime stopMeasurementTime simulationTime
	$qfile instvar pdepartures_   
	set stopPacketCount $pdepartures_
	puts "Throughput = [expr ($stopPacketCount-$startPacketCount)/(1024.0*1024*($stopMeasurementTime-$startMeasurementTime))*$packetSize*8] Mbps"
}

#$ns at $startMeasurementTime "startMeasurement"
#$ns at $stopMeasurementTime "stopMeasurement"

## no greater than 30*1000*1000 (30Mbps)
#$ns at 1 "$cbr change-rate [expr 9*1000*1000]";
#$ns at 12 "$cbr change-rate [expr 5*1000*1000]";
#$ns at 15 "$cbr change-rate [expr 21*1000*1000]";
#$ns at 21 "$cbr change-rate [expr 19*1000*1000]";
#$ns at 33 "$cbr change-rate [expr 1*1000*1000]";
#$ns at 50 "$cbr change-rate [expr 8*1000*1000]";
#$ns at 57 "$cbr change-rate [expr 12*1000*1000]";
#$ns at 69 "$cbr change-rate [expr 3*1000*1000]";
#$ns at 89 "$cbr change-rate [expr 9*1000*1000]";
#$ns at 102 "$cbr change-rate [expr 29*1000*1000]";
#$ns at 123 "$cbr change-rate [expr 16*1000*1000]";
#$ns at 158 "$cbr change-rate [expr 24*1000*1000]";
#$ns at 171 "$cbr change-rate [expr 28*1000*1000]";
#$ns at 189 "$cbr change-rate [expr 3*1000*1000]";
#$ns at 218 "$cbr change-rate [expr 8*1000*1000]";
#$ns at 293 "$cbr change-rate [expr 4*1000*1000]";
#
#				  
#$ns at $simulationTime "finish"
#
#$ns run
