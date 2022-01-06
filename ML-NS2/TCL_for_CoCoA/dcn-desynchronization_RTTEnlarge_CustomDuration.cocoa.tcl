#set N [lindex $argv 0];
#set N [expr $N];
set N 100;
set GROUP 10;
set lambda [expr [lindex $argv 0]];
set avg_ttl [expr [lindex $argv 1]];
set SEEDS [expr [lindex $argv 2]];

set NUM_BG 2;
set B 10000
set RTT 0.0001;	# default 0.0001
puts "The base RTT between the switch and receiver is $RTT"

set IP "127.0.0.1"
set PORT 10001


set raw_rtts(0) 2;
set rtts_counter 1;
set rtt_base 7;
set target_num_rtts [expr $rtt_base*10];

for {set i 3} {$rtts_counter < $target_num_rtts} {incr i} {

	set non_prime 0;
	for {set j 2} {$j<$i} {incr j} {

		if {[expr $i%$j] == 0} {

			set non_prime 1;
			break;	
		}	
	}
	if {$non_prime != 1} {

		set raw_rtts($rtts_counter) $i;
		incr rtts_counter;
		
		if {$rtts_counter == $target_num_rtts} {

			break;
		}
	}
}
#puts "number of rtts: $num_rtts";
#
#for {set i 0} {$i<$num_rtts} {incr i} {
#
#	puts "$rtts($i)";
#}
#set num_rtts [expr int(($num_rtts-1)*$randomize)];
#puts "num_rtts after randomize: $num_rtts";

set j 0;
set num_rtts 0;
for {set i 0} {$i<$rtts_counter} {incr j; incr num_rtts;} {

	set rtts($j) $raw_rtts($i);
	set i [expr $i+$rtt_base];
}

#set num_rtts 10;

set lineRate 1Gb;
set inputLineRate 10Gb;
set bottleneckRate 1Gb;
set simulationTime [expr [lindex $argv 3]];

set startMeasurementTime 1
set stopMeasurementTime 2
set flowClassifyTime 0.001

#set sourceAlg DC-TCP-Sack
if { $argc > 4 } {
	if { [string compare [lindex $argv 4] "Cubic"] == 0 } {
	
		set sourceAlg Cubic;
	} elseif { [string compare [lindex $argv 4] "Vegas"] == 0 } {
	
		set sourceAlg Vegas;
	} else {
	
		set sourceAlg Newreno
	}
} else {

	set sourceAlg Newreno
}
set switchAlg CoCoA
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

# CoCoA values
Queue/CoCoA set init_interval_ 1;
Queue/CoCoA set li_min_thresh_ [expr [lindex $argv 5]];

DelayLink set avoidReordering_ true

if {$enableNAM != 0} {
	set namfile [open out.nam w]
	$ns namtrace-all $namfile
}

#set mytracefile [open cwnd_[lindex $argv 0]-[lindex $argv 1].tr w]
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
	global ns N traceSamplingInterval tcp qfile packetSize
	
	set now [$ns now]
	
	for {set i 0} {$i < $N} {incr i} {
		set cwnd($i) [$tcp($i) set cwnd_]
	}
	
	puts -nonewline $file "$now $cwnd(0)"
	for {set i 1} {$i < $N} {incr i} {
		puts -nonewline $file " $cwnd($i)"
	}
	puts $file ""

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
	
	set cur_rtt [expr $RTT*$rtts([expr int($i/$GROUP)%($num_rtts)])/4];
	puts "cur_rtt@node($i):[expr $cur_rtt+$RTT/2]";
	set rtt_node($i) [expr $cur_rtt+$RTT/2];
	$ns duplex-link $s($i) $s_tor $inputLineRate [expr $cur_rtt/4] DropTail
	$ns duplex-link-op $s($i) $s_tor queuePos 0.25

	$ns duplex-link $r($i) $r_tor $lineRate [expr $cur_rtt/4] DropTail
	$ns duplex-link-op $r($i) $r_tor queuePos 0.25
}

#$ns duplex-link $node_bg $nqueue $inputLineRate [expr $RTT/8] DropTail
#$ns duplex-link-op $node_bg $nqueue queuePos 0.25

$ns simplex-link $s_tor $r_tor $bottleneckRate [expr $RTT/4] $switchAlg
$ns simplex-link $r_tor $s_tor $bottleneckRate [expr $RTT/4] DropTail
$ns queue-limit $s_tor $r_tor $B;
$ns queue-limit $r_tor $s_tor $B;

$ns duplex-link-op $s_tor $r_tor color "green"
$ns duplex-link-op $s_tor $r_tor queuePos 0.25
#set qfile [$ns monitor-queue $nqueue $nclient [open queue.tr w] $traceSamplingInterval]
#$ns at $traceSamplingInterval "myTrace $mytracefile"
#$ns at $throughputSamplingInterval "throughputTrace $throughputfile"

#set udp [new Agent/UDP];
#set null [new Agent/Null];
#$udp set fid_ [expr $N];
#$ns attach-agent $mid_node $udp;
#$ns attach-agent $nclient $null;
#$ns connect $udp $null;
#set cbr [new Application/Traffic/CBR];
#$cbr attach-agent $udp;
#$cbr set packetSize_ 1460;
#$cbr set rate_ $bgRate;
#$cbr set random_ false;
#$ns at 0.001 "$cbr start"

#for {set i 1} {$i < $simulationTime} {incr i} {
#
#	if { [expr int(fmod($i,2))] == 1} {
#
#		$ns at $i "$cbr start";
#		puts "ns starts @$i";
#	} else {
#	
#		$ns at $i "$cbr stop";
#		puts "ns stops @$i";
#	}
#}

set rng [new RNG]
$rng seed $SEEDS
set interval [new RandomVariable/Uniform]
$interval use-rng $rng
$interval set min_ 0.0
$interval set max_ 1.0

set flt [new RandomVariable/Exponential]
$flt use-rng $rng
$flt set avg_ $avg_ttl

set nts [new RandomVariable/Normal]
$nts use-rng $rng
$nts set avg_ 0.0;
$nts set std_ 5; 

set num_flow_ 0;

for {set i 0} {[expr $i*1.0/100]<$simulationTime} {incr i} {

#	puts "[expr $i*1.0/100]";
	set cur_time [expr $i*1.0/100];
	set temp_loc [expr int($i*1.0/20)];

	set remain [expr $temp_loc%$N];
	set base [expr $temp_loc/$N];
	if {[expr $base%2]==0} {

		
		set cur_loc [expr int($remain+[$nts value])];
	} else {
		
		set cur_loc [expr int(($N-1)-$remain+[$nts value])];
	}
	if {$cur_loc < 0} {

		set cur_loc 0;
		continue;
	} elseif {$cur_loc > [expr $N-1]} {

		set cur_loc [expr $N-1];
		continue;
	}


	set sentdata [$interval value];
	if { $sentdata <= $lambda } {

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
	
		$ns attach-agent $s($cur_loc) $tcp($num_flow_)
		$ns attach-agent $r($cur_loc) $sink($num_flow_)
		
		$tcp($num_flow_) set fid_ [expr $num_flow_]
		$sink($num_flow_) set fid_ [expr $num_flow_]
	
		$ns connect $tcp($num_flow_) $sink($num_flow_);

		set ftp($num_flow_) [new Application/FTP];
		$ftp($num_flow_) attach-agent $tcp($num_flow_);

		set st [expr $i*1.0/100];
		set et [expr $st+[$flt value]];
		$ns at $st "$ftp($num_flow_) start"
		$ns at $et "$ftp($num_flow_) stop";

#		$ns at $et "$ns detach-agent $s($cur_loc) $tcp($num_flow_); $ns detach-agent $r($cur_loc) $sink($num_flow_)";

#		puts "flow($num_flow_)@sender($cur_loc) at $st starts, ends at $et"
		puts "[expr int($st)]\t$rtt_node($cur_loc)"

		incr num_flow_;
	}
}


$tcp(0) setsocket $IP $PORT;

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

$defaultRNG seed $SEEDS
$ns run
