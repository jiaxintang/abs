set N [lindex $argv 0];
set N [expr $N];
set B 10000
set RTT 0.0001;	# default 0.0001
set ROUND 10;
set HOPS 10;

set IP "127.0.0.1"
set PORT 10000

set lineRate 1Gb;
set inputLineRate 1Gb;
set bottleneckRate 1Gb;
set simulationTime 90;

set startMeasurementTime 1
set stopMeasurementTime 2
set flowClassifyTime 0.001

#set sourceAlg DC-TCP-Sack
set sourceAlg Newreno
set switchAlg RED
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
set enableNAM 1

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

#set mytracefile [open cwnd_drop_[lindex $argv 0]-[lindex $argv 1].tr w]
#set throughputfile [open thrfile_[lindex $argv 0]-[lindex $argv 1].tr w]

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

set nqueue [$ns node]
set nclient [$ns node]

for {set i 0} {$i < $HOPS} {incr i} {

	set h($i) [$ns node];
}

$nqueue color red
$nqueue shape box
$nclient color blue

for {set i 0} {$i < $N} {incr i} {
	$ns duplex-link $n($i) $nqueue $inputLineRate [expr $RTT/8] DropTail
	$ns duplex-link-op $n($i) $nqueue queuePos 0.25
}

for {set i 0} {$i < [expr $HOPS-1]} {incr i} {

	$ns duplex-link $h($i) $h([expr $i+1]) $inputLineRate [expr $RTT] DropTail;
	$ns duplex-link-op $h($i) $h([expr $i+1]) queuePos 0.25;
	$ns duplex-link $h($i) $nclient $inputLineRate [expr $RTT] DropTail;
	$ns duplex-link-op $h($i) $nclient queuePos 0.25;
	$ns queue-limit $h($i) $h([expr $i+1]) 2;
	$ns queue-limit $h($i) $nclient 2;
}

# first and last nodes
$ns simplex-link $nqueue $h(0) $bottleneckRate [expr $RTT/8] $switchAlg;
$ns simplex-link $h(0) $nqueue $bottleneckRate [expr $RTT/8] DropTail;
$ns queue-limit $nqueue $h(0) $B;
$ns duplex-link-op $nqueue $h(0) color "green"
$ns duplex-link-op $nqueue $h(0) queuePos 0.25

$ns duplex-link $h([expr $HOPS-1]) $nclient $inputLineRate [expr $RTT] DropTail;
$ns duplex-link-op $h([expr $HOPS-1]) $nclient queuePos 0.25;
$ns queue-limit $h([expr $HOPS-1]) $nclient 2;	

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

$tcp(0) setsocket $IP $PORT;

#$ns at $traceSamplingInterval "myTrace $mytracefile"
#$ns at $throughputSamplingInterval "throughputTrace $throughputfile"

# Multipath Configuration
#$ns rtproto DV
#Agent/rtProto/DV set advertInterval 16

$ns rtproto Session
#Node set multiPath_ 1
#
#for {set i 0} {$i<[expr $HOPS-1]} {incr i} {
#	$ns cost $h($i) $h([expr $i+1]) 1;
#	$ns cost $h([expr $i+1]) $h($i) 1;
#	$ns cost $h($i) $nclient [expr $HOPS-$i];
#	$ns cost $nclient $h($i) [expr $HOPS-$i];
##	$ns link $h($i) $nclient] cost? ;
#}
#$ns cost $h([expr $HOPS-1]) $nclient 1;
#$ns cost $h([expr $HOPS-1]) $nclient 1;

for {set i 0} {$i<[expr $HOPS-1]} {incr i} {
	$ns rtmodel-at [expr 0.2+$i/5.0] down $h($i) $nclient;
}

set ru [new RandomVariable/Uniform]
$ru set min_ 0
$ru set max_ 1.0

for {set i 0} {$i < $N} {incr i} {
	$ns at 0.0 "$ftp($i) start";
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

$ns at $simulationTime "finish"

$ns run
