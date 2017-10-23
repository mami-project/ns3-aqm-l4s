import matplotlib.pyplot as plt
import sys

plttimeL = []
pltcwndL = []
with open("cwndL4S", "r") as file:
	for i in range(0, 40):
		file.readline()
	for line in file:
       		plttimeL.append(int(line[3:].split(" ")[0])/250.0)
		pltcwndL.append(int(line.split(" ")[2]))

plttimeC = []
pltcwndC = []
with open("cwndClassic", "r") as file:
	for i in range(0, 40):
		file.readline()
	for line in file:
       		plttimeC.append(int(line[3:].split(" ")[0])/250.0)
		pltcwndC.append(int(line.split(" ")[2]))

time = max(max(plttimeL), max(plttimeC))

plt.title("Congestion Window")
plt.ylim([0,500])
plt.xlim([0,time])
plt.plot(plttimeL, pltcwndL, label='L4S')
plt.plot(plttimeC, pltcwndC, label='Classic')
legend = plt.legend(loc='upper right')
plt.ylabel('Congestion Window')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/cwnd-both.png")

plt.clf()
plt.title("Congestion Window L4S Sender")
plt.ylim([0,500])
plt.xlim([0,time])
plt.plot(plttimeL, pltcwndL)
plt.ylabel('Congestion Window')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/cwnd-l4s.png")

plt.clf()
plt.title("Congestion Window Classic Sender")
plt.ylim([0,500])
plt.xlim([0,time])
plt.plot(plttimeC, pltcwndC, color='g')
plt.ylabel('Congestion Window')
plt.xlabel('Time [s]')
plt.grid()
plt.savefig(str(sys.argv[1])+"/cwnd-classic.png")
