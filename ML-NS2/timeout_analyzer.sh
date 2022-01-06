#!/bin/sh
#!/bin/bash

NUM_FLOWS=10
for ((i=0; i<$[${NUM_FLOWS}]; i++ ))
{

	cat traceall.tr | grep 'tcp' | grep '^+' | grep 'N '${i}' ' > flow_${i}.tr
	cat flow_${i}.tr | awk 'NR==1{previous=0;next}
		{
			if(($2-previous)>=0.2) {

				print "flow #"$8" timeout: "previous"->"$2; 
			}
			previous=$2;
		}{}'
#	echo "$(date): flow #.${i} is finished."
}

rm flow_*.tr
