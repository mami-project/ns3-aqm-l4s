import matplotlib.pyplot as plt
import numpy as np
import sys
import os
from scipy import integrate
#import mysql.connector

infile = str(sys.argv[1])+"/averages"

#parse name
aqm=infile.split('-')[0]+"-"+infile.split('-')[1]
nLF=int(infile.split('-')[2].split(':')[0])
nCF=int(infile.split('-')[2].split(':')[1])
ccC=infile.split('-')[3]
ccL=infile.split('-')[4]
ecn=(infile.split('-')[5]=="ecn")
qDelayRef=float(infile.split('-')[6].split('/')[0])

#parse results
with open (infile, "r") as myfile:
	myfile.readline()
	line = myfile.readline()
	qDelayMaxL=float(line.split(',')[0].split(':')[1])
	qDelayAvgL=float(line.split(',')[1].split(':')[1])
	packets0L=int(line.split(',')[2].split(':')[1])
	packetsL=int(line.split(',')[3].split(':')[1])
	line = myfile.readline()
	qDelayMaxC=float(line.split(',')[0].split(':')[1])
	qDelayAvgC=float(line.split(',')[1].split(':')[1])
	packets0C=int(line.split(',')[2].split(':')[1])
	packetsC=int(line.split(',')[3].split(':')[1])

	myfile.readline()
	line=myfile.readline()
	throughputL=float(line.split(',')[0].split(':')[1])
	throughputC=float(line.split(',')[1].split(':')[1])
	throughputS=float(line.split(',')[2].split(':')[1])

	myfile.readline()
	line=myfile.readline()
	marksL=int(line.split(',')[0].split(':')[1])
	dropsL=int(line.split(',')[1].split(':')[1])
	odropsL=int(line.split(',')[2].split(':')[1])
	fdropsL=int(line.split(',')[3].split(':')[1])
	line=myfile.readline()
	marksC=int(line.split(',')[0].split(':')[1])
	dropsC=int(line.split(',')[1].split(':')[1])
	odropsC=int(line.split(',')[2].split(':')[1])
	fdropsC=int(line.split(',')[3].split(':')[1])

import MySQLdb

db = MySQLdb.connect(host="localhost",    # your host, usually localhost
                     user="root",         # your username
                     passwd="1234",  # your password
                     db="l4s")        # name of the data base

cur = db.cursor()

cur.execute("INSERT INTO results (aqm, nLF, nCF, ccC, ccL, ecn, qDelayRef, qDelayMaxL, qDelayAvgL, packets0L, packetsL, qDelayMaxC, qDelayAvgC, packets0C, packetsC, throughputL, throughputC, throughputS, marksL, dropsL, odropsL, fdropsL, marksC, dropsC, odropsC, fdropsC) VALUES ('"+str(aqm)+"',"+str(nLF)+","+str(nCF)+",'"+str(ccC)+"','"+str(ccL)+"',"+str(ecn)+","+str(qDelayRef)+","+str(qDelayMaxL)+","+str(qDelayAvgL)+","+str(packets0L)+","+str(packetsL)+","+str(qDelayMaxC)+","+str(qDelayAvgC)+","+str(packets0C)+","+str(packetsC)+","+str(throughputL)+","+str(throughputC)+","+str(throughputS)+","+str(marksL)+","+str(dropsL)+","+str(odropsL)+","+str(fdropsL)+","+str(marksC)+","+str(dropsC)+","+str(odropsC)+","+str(fdropsC)+");")

db.commit()

db.close()



















