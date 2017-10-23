import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import MySQLdb
import sys


# mysql connector
db = MySQLdb.connect(host="localhost", user="root", passwd="1234", db="l4s")
cur = db.cursor()


# parse argument
ccC=str(sys.argv[1])
ccCstr=ccC.title()

ccL=str(sys.argv[2])
if (ccL=='dctcp'): ccLstr="DCTCP"
else: ccLstr=ccL.title()

ecn=str(sys.argv[3])
if (ecn=='1'): ecnstr="ECN on Classic flows: On"
else: ecnstr="ECN on Classic flows: Off"
if (ecn=='1'): ecnstr2="ecn"
else: ecnstr2="noecn"

print("AQM Comparison: "+ccCstr+" - "+ccLstr+" - "+ecnstr)



# set up data storage
index=[]
indexSca=[]
labels=[]
labelsSca=[]
aqms=[]

markRateL=[]
faultRateC=[]
faultRateCSca=[]
markRateLSca=[]

dropRateC=[]
markRateC=[]

qDelayMaxL=[]
qDelayAvgL=[]
qDelayMaxLSca=[]
qDelayAvgLSca=[]

emptyL=[]
emptyC=[]
emptyB=[]
emptyLSca=[]
emptyCSca=[]
emptyBSca=[]

utilization=[]
jainmod=[]
utilizationSca=[]
jainmodSca=[]


# initialize counters
indexCounter=0
subText="test"


# access database
for nLF in [1, 5, 10]:
	for nCF in [1, 5, 10]:
		labels.append("\nClassic Flows: "+str(nCF)+"\nLL Flows: "+str(nLF)+"\n")
		for aqm in ["sq-red", "sq-pie"]:
			aqms.append(aqm)
			cur.execute("SELECT markRateL, faultRateC, qDelayMaxL, qDelayAvgL, shareqEmptyL, shareqEmptyC, shareqEmptyB, utilization, jainmod, dropRateC, markRateC FROM results WHERE aqm='"+aqm+"' AND nLF="+str(nLF)+" AND nCF="+str(nCF)+" AND ccC='"+ccC+"' AND ccL='"+ccC+"' AND ecn="+str(ecn)+";")
			rows=cur.fetchall()
			if (len(rows)==0): rows=[[0,0]]
			for row in rows:
				index.append(indexCounter)
				markRateL.append(float(row[0]))
				faultRateC.append(float(row[1]))
				qDelayMaxL.append(float(row[2]))
				qDelayAvgL.append(float(row[3]))
				emptyL.append(float(row[4]))
				emptyC.append(float(row[5]))
				emptyB.append(float(row[6]))
				utilization.append(float(row[7]))
				jainmod.append(float(row[8]))
				dropRateC.append(float(row[9]))
				markRateC.append(float(row[10]))
				indexCounter+=1
"""
for nLF in [1, 5, 10]:
	for nCF in [1, 5, 10]:
		labelsSca.append("L4S Flows: "+str(nLF)+"\nClassic Flows: "+str(nCF)+"\n")
		for aqm in ["dq-red-sca", "dq-pie-sca", "l4s-cred", "l4s-pi2", "l4s-pie"]:
			aqms.append(aqm)
			cur.execute("SELECT markRateL, faultRateC, qDelayMaxL, qDelayAvgL, shareqEmptyL, shareqEmptyC, shareqEmptyB, utilization, jainmod FROM results WHERE aqm='"+aqm+"' AND nLF="+str(nLF)+" AND nCF="+str(nCF)+" AND ccC='"+ccC+"' AND ccL='"+ccL+"' AND ecn="+str(ecn)+";")
			rows=cur.fetchall()
			if (len(rows)==0): rows=[[0,0]]
			for row in rows:
				indexSca.append(indexCounter)
				markRateLSca.append(float(row[0]))
				faultRateCSca.append(float(row[1]))
				qDelayMaxLSca.append(float(row[2]))
				qDelayAvgLSca.append(float(row[3]))
				emptyLSca.append(float(row[4]))
				emptyCSca.append(float(row[5]))
				emptyBSca.append(float(row[6]))
				utilizationSca.append(float(row[7]))
				jainmodSca.append(float(row[8]))
				indexCounter+=1
"""



# set up plot frame
aqmname=["RED", "PIE"]
colors = cm.Set1(np.linspace(0, 1, 9))

fontsizetitle=18
fontsizelabel=10
fontsizelegend=10
title="Single Queue AQM\nCongestion Control: "+ccCstr+"  -  "+ecnstr+"\n\n"
basename="plots/sq-per-tcpconf-"+ccC+"-"+ccL+"-"+ecnstr2+".png"

# drop rate
f, axarr = plt.subplots(2)
plt.subplots_adjust(left=None, bottom=None, right=None, top=None, wspace=None, hspace=0.35)
axarr[0].set_xticks(np.arange(0.5, len(labels), 1.0))
axarr[0].set_xticklabels(labels)
axarr[0].set_title("Queue Delay", fontsize=14)
axarr[0].set_ylabel("Queue Delay [ms]\n", fontsize=12)
axarr[1].set_xticks(np.arange(0.5, len(labels), 1.0))
axarr[1].set_xticklabels(labels)
axarr[1].set_title("Utilization and Modified Jain Index", fontsize=14)
axarr[1].set_ylabel("Utilization and Modified Jain Index [1]\n", fontsize=12)
axarr[1].text(0.27125, 0.5, "Classic more throughput", bbox=dict(facecolor='w', alpha=0.2), fontsize=10, horizontalalignment='left', verticalalignment='center')
axarr[1].text(0.27125, 1.5, "LL more throughput     ", bbox=dict(facecolor='w', alpha=0.2), fontsize=10, horizontalalignment='left', verticalalignment='center')


f.add_subplot(111, frameon=False)
plt.tick_params(labelcolor='none', top='off', bottom='off', left='off', right='off')
plt.title(title, fontsize=fontsizetitle)

#plt.xlim([0,len(labels)])

# ploting
for i in range(0,2):
	axarr[0].plot(np.arange(0.5, len(labels), 1.0), qDelayMaxL[i::2], linestyle=':', marker='+', markersize=12, color=colors[i], label="Maximum Queue Delay - "+aqmname[i])
	axarr[0].plot(np.arange(0.5, len(labels), 1.0), qDelayAvgL[i::2], linestyle='-', marker='+', markersize=12, color=colors[i], label="Average Queue Delay - "+aqmname[i])

for i in range(0,2):
	axarr[1].plot(np.arange(0.5, len(labels), 1.0), utilization[i::2], linestyle=':', marker='+', markersize=12, color=colors[i], label="Link Utilization - "+aqmname[i])
	axarr[1].plot(np.arange(0.5, len(labels), 1.0), jainmod[i::2], linestyle='-', marker='+', markersize=12, color=colors[i], label="Modified Jain Index - "+aqmname[i])

axarr[0].set_ylim([0,100])
#axarr[0].set_xlim([0,len(labels)])
axarr[0].tick_params(axis='x',which=u'both',length=0)
axarr[0].legend(loc='upper center', ncol=4, fontsize=fontsizelegend)
axarr[0].yaxis.grid(linestyle=':')
axarr[1].set_ylim([0,2])
axarr[1].tick_params(axis='x',which=u'both',length=0)
axarr[1].legend(loc='upper center', ncol=4, fontsize=fontsizelegend)
axarr[1].yaxis.grid(linestyle=':')
#axarr[1].set_xlim([0,len(labels)])

f.set_size_inches(15, 10)
f.savefig(basename, dpi=100)
#plt.show()










