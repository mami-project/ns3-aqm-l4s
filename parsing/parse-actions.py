import matplotlib.pyplot as plt
import numpy as np
from scipy import integrate
import sys
import heapq

actionsL = []
actionsC = []
timeL = []
timeC = []
with open("actionsL4S", "r") as file:
	for line in file:
		timeL.append(float(line.split(",")[0]))
		actionsL.append(line.split(",")[1][0])

with open("actionsClassic", "r") as file:
	for line in file:
		timeC.append(float(line.split(",")[0]))
		actionsC.append(line.split(",")[1][0])

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

marksL = actionsL[startL:stopL].count('M')
marksC = actionsC[startC:stopC].count('M')
dropsL = actionsL[startL:stopL].count('D')
dropsC = actionsC[startC:stopC].count('D')
fdropsL = actionsL[startL:stopL].count('F')
fdropsC = actionsC[startC:stopC].count('F')
odropsL = actionsL[startL:stopL].count('O')
odropsC = actionsC[startC:stopC].count('O')


f = open(str(sys.argv[1])+"/averages", 'a')
f.write("actions\nmarksL:"+str(marksL)+",dropsL:"+str(dropsL + odropsL + fdropsL)+",odropsL:"+str(odropsL) + ",fdropsL:"+str(fdropsL)+"\n")
f.write("marksC:"+str(marksC)+",dropsC:"+str(dropsC + odropsC + fdropsC)+",odropsC:"+str(odropsC) + ",fdropsC:"+str(fdropsC)+"\n")


