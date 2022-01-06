#!/bin/sh
#!/bin/bash

echo "source ${1}_pre.tcl" > ${1}.tcl
echo "source red.tcl;" >> ${1}.tcl

cat ${1}.tr | awk 'NR==0{tt=0; gra=0.01; next}
{
	print "$ns at " $1-gra " \"$cbr change-rate [expr $baseline*1000*1000-" $2 "*1000*1000]\""
	
} END{ }' >> ${1}.tcl

echo "\$ns at \$simulationTime \"finish\"" >> ${1}.tcl
echo "\$ns run" >> ${1}.tcl
