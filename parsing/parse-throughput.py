import matplotlib.pyplot as plt
import numpy as np
from scipy import integrate
import sys

time = []
throughputL = []
throughputC = []
throughputS = []
with open("throughput", "r") as file:
	for line in file:
		time.append(float(line.split(',')[0]))
		throughputL.append(float(line.split(',')[1]))
		throughputC.append(float(line.split(',')[2]))
		throughputS.append(float(line.split(',')[1]) + float(line.split(',')[2]))

try:
	start = next(x[0] for x in enumerate(time) if x[1] >= 30)
except:
	start = 0
try:
	stop = next(x[0] for x in enumerate(time) if x[1] >= 90)
except:
	stop = len(time)
times = time[start:stop]
throughputLs = throughputL[start:stop]
throughputCs = throughputC[start:stop]
throughputSs = throughputS[start:stop]

avgL = sum(throughputLs)/float(len(throughputLs))
avgC = sum(throughputCs)/float(len(throughputCs))
avgS = sum(throughputSs)/float(len(throughputSs))

f = open(str(sys.argv[1])+"/averages", 'a')
f.write("throughput\navgL:"+str(avgL)+",avgC:"+str(avgC)+",avgS:"+str(avgS)+"\n")

throughputRange = (int(max(throughputS)/5)+2)*5

plt.title("Throughput")
plt.ylim([0,throughputRange])
plt.xlim([0,len(time)])
plt.plot(time, throughputL, label='L4S')
plt.plot(time, throughputC, label='Classic')
plt.plot(time, throughputS, label='Sum')
legend = plt.legend(loc='upper right')
plt.ylabel('Throughput [Mbps]')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/throughput-both.png")

plt.clf()
plt.title("Throughput L4S Queue")
plt.ylim([0,throughputRange])
plt.xlim([0,len(time)])
plt.plot(time, throughputL)
plt.ylabel('Throughput [Mbps]')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/throughput-l4s.png")

plt.clf()
plt.title("Throughput Classic Queue")
plt.ylim([0,throughputRange])
plt.xlim([0,len(time)])
plt.plot(time, throughputC, color='g')
plt.ylabel('Throughput [Mbps]')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/throughput-classic.png")


