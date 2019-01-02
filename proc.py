import matplotlib.pyplot as plt
f = open("proc.txt","r")
x = []
y = []
for i in range(53):
    x.append(0)
    y.append(0)
    x[i],y[i] = [int(x) for x in f.readline().split()]

plt.xlabel("Process ID")
plt.ylabel("Creation Time")
plt.grid()
plt.plot(x[2:],y[2:])
plt.show()
f.close()