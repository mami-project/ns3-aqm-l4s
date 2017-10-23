import matplotlib.pyplot as plt
import numpy as np
from scipy import integrate
import sys
import heapq

timeL = []
timeC = []
delayL = []
delayC = []
lenL = []
lenC = []
with open("qDelayL4S", "r") as file:
	for line in file:
		timeL.append(float(line.split(",")[0]))
		delayL.append(float(line.split(",")[1])*1000)
		lenL.append(int(line.split(",")[2]))

with open("qDelayClassic", "r") as file:
	for line in file:
		timeC.append(float(line.split(",")[0]))
		delayC.append(float(line.split(",")[1])*1000)
		lenC.append(int(line.split(",")[2]))

try:
	startL = next(x[0] for x in enumerate(timeL) if x[1] >= 30)
except:
	startL = 0
try:
	stopL = next(x[0] for x in enumerate(timeL) if x[1] >= 90)
except:
	stopL = len(timeL)
try:
	startC = next(x[0] for x in enumerate(timeC) if x[1] >= 30)
except:
	startC = 0
try:
	stopC = next(x[0] for x in enumerate(timeC) if x[1] >= 90)
except:
	stopC = len (timeC)

dLs = delayL[startL:stopL]
dCs = delayC[startC:stopC]
lLs = lenL[startL:stopL]
lCs = lenC[startC:stopC]
maxL = max(dLs)
maxC = max(dCs)
avgL = sum(dLs)/float(len(dLs))
avgC = sum(dCs)/float(len(dCs))
n0L = dLs.count(0)
n0C = dCs.count(0)

f = open(str(sys.argv[1])+"/averages", 'w')
f.write("qDelay\nmaxL:" + str(maxL) + ",avgL:"+str(avgL) + ",packets0L:" + str(n0L) + ",packetsL:" + str(len(dLs)))
f.write("\nmaxC:" + str(maxC) + ",avgC:"+str(avgC) + ",packets0C:" + str(n0C) + ",packetsC:" + str(len(dCs)) + "\n")


plttime = max(max(timeL), max (timeC))

plt.title("Queue Delay")
plt.ylim([0,500])
plt.xlim([0,plttime])
plt.plot(timeL, delayL, label='L4S')
plt.plot(timeC, delayC, label='Classic')
legend = plt.legend(loc='upper right')
plt.ylabel('Queue Delay [ms]')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/qDelay-both.png")

plt.clf()
plt.title("Queue Delay L4S Queue")
plt.ylim([0,100])
plt.xlim([0,plttime])
plt.plot(timeL, delayL)
plt.ylabel('Queue Delay [ms]')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/qDelay-l4s.png")

plt.clf()
plt.title("Queue Delay Classic Queue")
plt.xlim([0,plttime])
plt.plot(timeC, delayC, color='g')
plt.ylabel('Queue Delay [ms]')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/qDelay-classic.png")


