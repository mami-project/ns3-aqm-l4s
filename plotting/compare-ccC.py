import matplotlib.pyplot as plt
import numpy as np
import MySQLdb
import sys


# mysql connector
db = MySQLdb.connect(host="localhost", user="root", passwd="1234", db="l4s")
cur = db.cursor()

qDelayAvgL0=[]
qDelayAvgC0=[]
qDelayMaxL0=[]
qDelayMaxC0=[]
shareL0=[]
markRateL0=[]
faultRateC0=[]
utilization0=[]

qDelayAvgL1=[]
qDelayAvgC1=[]
qDelayMaxL1=[]
qDelayMaxC1=[]
shareL1=[]
markRateL1=[]
faultRateC1=[]
utilization1=[]

qDelayAvgLr=[]
qDelayAvgCr=[]
qDelayMaxLr=[]
qDelayMaxCr=[]
shareLr=[]
markRateLr=[]
faultRateCr=[]
utilizationr=[]


# access database
for aqm in ["sq-red", "sq-pie", "dq-red", "dq-pie"]:
	for ecn in ["0","1"]:
		for nLF in [1,5,10]:
			for nCF in [1,5,10]:
				cur.execute("SELECT qDelayAvgL, qDelayMaxL, qDelayAvgC, qDelayMaxC, shareL, utilization, markRateL, faultRateC FROM results WHERE aqm='"+aqm+"' AND nLF="+str(nLF)+" AND nCF="+str(nCF)+" AND ccC='cubic' AND ccL='cubic' AND ecn="+ecn+";")
				rows=cur.fetchall()
				row=rows[0]
				qDelayAvgL0.append(float(row[0]))
				qDelayMaxL0.append(float(row[1]))
				qDelayAvgC0.append(float(row[2]))
				qDelayMaxC0.append(float(row[3]))
				shareL0.append(float(row[4]))
				utilization0.append(float(row[5]))
				markRateL0.append(float(row[6]))
				faultRateC0.append(float(row[7]))

				cur.execute("SELECT qDelayAvgL, qDelayMaxL, qDelayAvgC, qDelayMaxC, shareL, utilization, markRateL, faultRateC FROM results WHERE aqm='"+aqm+"' AND nLF="+str(nLF)+" AND nCF="+str(nCF)+" AND ccC='reno' AND ccL='reno' AND ecn="+ecn+";")
				rows=cur.fetchall()
				row=rows[0]
				qDelayAvgL1.append(float(row[0]))
				qDelayMaxL1.append(float(row[1]))
				qDelayAvgC1.append(float(row[2]))
				qDelayMaxC1.append(float(row[3]))
				shareL1.append(float(row[4]))
				utilization1.append(float(row[5]))
				markRateL1.append(float(row[6]))
				faultRateC1.append(float(row[7]))



for aqm in ["dq-red-sca", "dq-pie-sca", "l4s-cred", "l4s-pi2", "l4s-pie"]:
	for ecn in ["0", "1"]:
		for nLF in [1,5,10]:
			for nCF in [1,5,10]:
				for ccL in ["dctcp", "relentless"]:
					cur.execute("SELECT qDelayAvgL, qDelayMaxL, qDelayAvgC, qDelayMaxC, shareL, utilization, markRateL, faultRateC FROM results WHERE aqm='"+aqm+"' AND nLF="+str(nLF)+" AND nCF="+str(nCF)+" AND ccC='cubic' AND ccL='"+ccL+"' AND ecn="+ecn+";")
					rows=cur.fetchall()
					row=rows[0]
					qDelayAvgL0.append(float(row[0]))
					qDelayMaxL0.append(float(row[1]))
					qDelayAvgC0.append(float(row[2]))
					qDelayMaxC0.append(float(row[3]))
					shareL0.append(float(row[4]))
					utilization0.append(float(row[5]))
					markRateL0.append(float(row[6]))
					faultRateC0.append(float(row[7]))

					cur.execute("SELECT qDelayAvgL, qDelayMaxL, qDelayAvgC, qDelayMaxC, shareL, utilization, markRateL, faultRateC FROM results WHERE aqm='"+aqm+"' AND nLF="+str(nLF)+" AND nCF="+str(nCF)+" AND ccC='reno' AND ccL='"+ccL+"' AND ecn="+ecn+";")
					rows=cur.fetchall()
					row=rows[0]
					qDelayAvgL1.append(float(row[0]))
					qDelayMaxL1.append(float(row[1]))
					qDelayAvgC1.append(float(row[2]))
					qDelayMaxC1.append(float(row[3]))
					shareL1.append(float(row[4]))
					utilization1.append(float(row[5]))
					markRateL1.append(float(row[6]))
					faultRateC1.append(float(row[7]))





for i in range(0, len(qDelayAvgL0)):
	qDelayAvgLr.append(qDelayAvgL1[i]-qDelayAvgL0[i])
	qDelayAvgCr.append(qDelayAvgC1[i]-qDelayAvgC0[i])
	qDelayMaxLr.append(qDelayMaxL1[i]-qDelayMaxL0[i])
	qDelayMaxCr.append(qDelayMaxC1[i]-qDelayMaxC0[i])
	shareLr.append(shareL1[i]-shareL0[i])
	utilizationr.append(utilization1[i]-utilization0[i])
	markRateLr.append(markRateL1[i]-markRateL0[i])
	faultRateCr.append(faultRateC1[i]-faultRateC0[i])


# change in bw division
f, (ax1, ax2) = plt.subplots(1, 2, sharey=True)

ax1.plot(np.arange(0.5, 72.0, 1.0), shareLr[:72], linestyle='', marker='+', color='b')
ax1.plot(np.arange(0.5, 72.0, 1.0), utilizationr[:72], linestyle='', marker='+', color='g')
ax1.set_xlim([0,72])
ax1.set_ylim([-3,3])
ax1.set_xticks(np.arange(9, 72.0, 18.0))
ax1.set_xticklabels(["sq-red", "sq-pie", "dq-red", "dq-pie"], fontsize=10)
ax1.set_ylabel("Difference in [1]", fontsize=10)
ax1.plot([0, 72], [0, 0], linestyle=':', color='k')
for i in range(1,4):
	ax1.plot([18*i, 18*i], [-100, 100], linestyle=':', color='k', alpha=0.5)


ax2.plot(np.arange(0.5, 180.0, 1.0), shareLr[72:], linestyle='', marker='+', color='b', label="Throughput Share L4S (Reno - Cubic)")
ax2.plot(np.arange(0.5, 180.0, 1.0), utilizationr[72:], linestyle='', marker='+', color='g', label="Link Utilization (Reno - Cubic)")
ax2.set_xlim([0,180])
ax2.set_ylim([-3,3])
ax2.set_xticks(np.arange(18.0, 180.0, 36.0))
ax2.set_xticklabels(["dq-red-sca", "dq-pie-sca", "l4s-cred", "l4s-pi2", "l4s-pie"], fontsize=10)
ax2.plot([0, 180], [0, 0], linestyle=':', color='k')
for i in range(1,5):
	ax2.plot([36*i, 36*i], [-100, 100], linestyle=':', color='k', alpha=0.5)
ax2.legend(loc='upper right', ncol=1, fontsize=10)

f.suptitle("Difference in Throughput Share L4S and Utilization (Reno - Cubic)", fontsize=14)
f.set_size_inches(12, 8)
f.savefig("plots-comparison/compare-ccC-throughput.png", dpi=100)


# queue delay
plt.clf()
f, (ax1, ax2) = plt.subplots(1, 2, sharey=True)

ax1.plot(np.arange(0.5, 72.0, 1.0), qDelayAvgLr[:72], linestyle='', marker='+', color='g')
ax1.plot(np.arange(0.5, 72.0, 1.0), qDelayMaxLr[:72], linestyle='', marker='+', color='b')
ax1.set_xlim([0,72])
ax1.set_ylim([-100, 100])
ax1.set_xticks(np.arange(9, 72.0, 18.0))
ax1.set_xticklabels(["sq-red", "sq-pie", "dq-red", "dq-pie"], fontsize=10)
ax1.set_ylabel("Difference in [ms]", fontsize=10)
ax1.plot([0, 72], [0, 0], linestyle=':', color='k')
for i in range(1,4):
	ax1.plot([18*i, 18*i], [-100, 100], linestyle=':', color='k', alpha=0.5)


ax2.plot(np.arange(0.5, 180.0, 1.0), qDelayMaxLr[72:], linestyle='', marker='+', color='b', label="Maximum Queuing Delay (Reno - Cubic)")
ax2.plot(np.arange(0.5, 180.0, 1.0), qDelayAvgLr[72:], linestyle='', marker='+', color='g', label="Average Queuing Delay (Reno - Cubic)")
ax2.set_xlim([0,180])
ax2.set_ylim([-100, 100])
ax2.set_xticks(np.arange(18.0, 180.0, 36.0))
ax2.set_xticklabels(["dq-red-sca", "dq-pie-sca", "l4s-cred", "l4s-pi2", "l4s-pie"], fontsize=10)
ax2.plot([0, 180], [0, 0], linestyle=':', color='k')
for i in range(1,5):
	ax2.plot([36*i, 36*i], [-100, 100], linestyle=':', color='k', alpha=0.5)
ax2.legend(loc='upper right', ncol=1, fontsize=10)

f.suptitle("Difference in Average and Maximum L4S Queuing Delay (Reno - Cubic)", fontsize=14)
f.set_size_inches(12, 8)
f.savefig("plots-comparison/compare-ccC-qDelay.png", dpi=100)


#drop rates
plt.clf()
f, (ax1, ax2) = plt.subplots(1, 2, sharey=True)

ax1.plot(np.arange(0.5, 72.0, 1.0), markRateLr[:72], linestyle='', marker='+', color='b')
ax1.plot(np.arange(0.5, 72.0, 1.0), faultRateCr[:72], linestyle='', marker='+', color='g')
ax1.set_xlim([0,72])
ax1.set_ylim([-0.5, 0.5])
ax1.set_xticks(np.arange(9, 72.0, 18.0))
ax1.set_xticklabels(["sq-red", "sq-pie", "dq-red", "dq-pie"], fontsize=10)
ax1.set_ylabel("Difference in [1]", fontsize=10)
ax1.plot([0, 72], [0, 0], linestyle=':', color='k')
for i in range(1,4):
	ax1.plot([18*i, 18*i], [-100, 100], linestyle=':', color='k', alpha=0.5)


ax2.plot(np.arange(0.5, 180.0, 1.0), markRateLr[72:], linestyle='', marker='+', color='b', label="L4S Mark Rate (Reno - Cubic)")
ax2.plot(np.arange(0.5, 180.0, 1.0), faultRateCr[72:], linestyle='', marker='+', color='g', label="Classic Mark/Drop Rate (Reno - Cubic)")
ax2.set_xlim([0,180])
ax2.set_ylim([-0.5, 0.5])
ax2.set_xticks(np.arange(18.0, 180.0, 36.0))
ax2.set_xticklabels(["dq-red-sca", "dq-pie-sca", "l4s-cred", "l4s-pi2", "l4s-pie"], fontsize=10)
ax2.plot([0, 180], [0, 0], linestyle=':', color='k')
for i in range(1,5):
	ax2.plot([36*i, 36*i], [-100, 100], linestyle=':', color='k', alpha=0.5)
ax2.legend(loc='upper right', ncol=1, fontsize=10)

f.suptitle("Difference in Drop Rate (Reno - Cubic)", fontsize=14)
f.set_size_inches(12, 8)
f.savefig("plots-comparison/compare-ccC-dropRate.png", dpi=100)



















