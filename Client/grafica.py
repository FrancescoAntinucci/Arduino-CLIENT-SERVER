from matplotlib import pyplot as plt
from matplotlib import style
import matplotlib.animation as animation
import sys

style.use('seaborn')
figure=plt.figure()
ax1=figure.add_subplot(1,1,1)


x_graph=[] #lista dei tempi(compresi doppioni)
y_graph=[]  #lista dei valori
word=[]

x=[] #lista dei tempi(senza ripetizione)
y=[] #lista dei valori

nome=sys.argv[1]
nome=nome[:len(nome)-4]

def animazione(i):
	with open(sys.argv[1],"r") as data:
		sensore=""
		lines=data.readlines()
		n=0	
		for riga in lines:
			if len(riga)>1:
				word=riga.split(" ")
					
				if(word[0]=="tempo"):
					x_graph.append(float(word[1]))
				elif(word[0]==nome):
					y_graph.append(float(word[2]))
					sensore=word[0]+" "+word[3]
		
		#media delle temperature con lo stesso orario
		
		for elem in range(0,len(x_graph)):
			c=x_graph.count(x_graph[elem])
			
			if c>1:
				if x_graph[elem] in x:
					continue
				else:
					for s in range(elem,elem+c):
						n=n+y_graph[s]
					
					y.append(n/c)
					x.append(x_graph[elem])
					n=0
			else:
				x.append(x_graph[elem])
				y.append(y_graph[elem])
		
		ax1.clear()
		ax1.set_ylabel(sensore,fontsize=16.5)
		ax1.set_xlabel("Tempo(hour,min)",fontsize=16.5)
		titolo="Andamento nel tempo "+nome
		ax1.set_title(titolo,fontsize=23,color='r')
		ax1.plot(x,y)
		data.close()

animazione(0)
live=animation.FuncAnimation(figure,animazione,interval=2000)
mng=plt.get_current_fig_manager()
mng.window.showMaximized()
plt.show();



