set baseline 1;
set bw_upperbound [expr 2*1000*1000*1000]; 
#set lineRate $bw_upperbound;
#set inputLineRate $bw_upperbound;
set lineRate [expr 10*1000*1000*1000] ;
set inputLineRate $lineRate;
set bottleneckRate $bw_upperbound;
set bgRate [expr $bw_upperbound/2];
set simulationTime 300;
