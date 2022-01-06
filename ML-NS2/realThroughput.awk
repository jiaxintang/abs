#####the shell is to culculate instant throughput(average throughput in samplinginterval) of all flows#####
### Note: The file handled by this awk can not include blank record, or it will occurs error. 

BEGIN  {recievedPktNum=0;ktotalDataSize=0;a=0;firstJudge=0;samplingIntervalTime=0.1;time=0;packetNum=0;sendersNum=sendernum;packetSize=1500;RTT=0.0001 } {
#print $2 , time ;
a=int($2/samplingIntervalTime);
#print a, firstJudge;
if(a!=0&&firstJudge==0){
   for(i=0;i<a;i++){
      print time " " "0" " " "0" " " "0" " " "0" " " "0" " " packetSize " " "0" " " "0" " " "0"
      time=time+samplingIntervalTime;
   }
   firstJudge=1;
   packetNum=1;
   recievedPktNum=recievedPktNum+1;# count the number of packets has been received by receiver
} #if(a!=0&&firstJudge==0)
else if(($2-time)<=0.00000001){ # ($2-time)<=0.00000001 等价于 $2<=time -处理精度问题，取值区间是左开右闭：( ]
   packetNum=packetNum+1;# calculate the number of paket in samplingIntervalTime
   recievedPktNum=recievedPktNum+1;# count the number of packets has been received by receiver
} #else if($2<=time)
else{
   dataSize=packetSize*packetNum; # total data sent by on a sender (in byte)
   ktotalDataSize=ktotalDataSize+packetSize*packetNum/1000; # total data received by on a receiver (in KB)
   bdataSize=dataSize*8; # convert Byte to bit
   #plusing 0.0000002*packetNum solves the problem of precision caused by trace/xx.cc
   instant_throughput=bdataSize/(samplingIntervalTime)/1000/1000;#Mb/s instant throughput #+0.0000002
   if(instant_throughput>1000){
     instant_throughput=1000;
   }
  instant_throughputGbps=instant_throughput/1000
#print time " " instant_throughputGbps " " instant_throughput " " packetNum " " ktotalDataSize " " recievedPktNum " " packetSize " " bdataSize " " $2 " " NR
print time "\t" instant_throughputGbps
   time=time+samplingIntervalTime;
   if($2>time){
      b=int(($2-time)/samplingIntervalTime)
      if((($2-time)%samplingIntervalTime)!=0){ #acheive the ceil() function
         b=b+1
      }
      for(j=0;j<b;j++){
         print time " " "0" " " "0" " " "0" " " ktotalDataSize " " recievedPktNum " " packetSize " " "0" " " "0" " " "0"
         time=time+samplingIntervalTime;
      }
   } #if($2>time)
   packetNum=1;# the number of paket sent by senders
   recievedPktNum=recievedPktNum+1;# count the number of packets has been received by receiver
} #else

} #action
END {
   dataSize=packetSize*packetNum; # total data sent by on a sender (in byte)
   ktotalDataSize=ktotalDataSize+packetSize*packetNum/1000; # total data received by on a receiver (in KB)
   bdataSize=dataSize*8; # convert Byte to bit
   instant_throughput=bdataSize/samplingIntervalTime/1000/1000;#Mb/s instant throughput
   if(instant_throughput>1000){
     instant_throughput=1000;
   }
  instant_throughputGbps=instant_throughput/1000
#print time " " instant_throughputGbps " " instant_throughput " " packetNum " " ktotalDataSize " " recievedPktNum " " packetSize " " bdataSize " " $2 " " NR
print time "\t" instant_throughputGbps 
}
