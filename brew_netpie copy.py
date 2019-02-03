import microgear.client as microgear
import time
import logging

appid = 'Brew'
gearkey = '3aewfy0NnL6pFnZ'
gearsecret =  'JrF5MRNuP8nC5uKQWAraykXiQ'

microgear.create(gearkey,gearsecret,appid,{'debugmode': True})

def connection():
    logging.info("Now I am connected with netpie")

def subscription(topic,message):
    logging.info(topic+" "+message)

def disconnect():
    logging.debug("disconnect is work")

microgear.setalias("BrewGateway")
microgear.on_connect = connection
microgear.on_message = subscription
microgear.on_disconnect = disconnect
# microgear.subscribe("/brew/temperature")
microgear.connect(False)

while True:
	if(microgear.connected):
		# microgear.chat("doraemon","Hello world."+str(int(time.time())))
		microgear.publish("/brew/temperature","28.5",{'retain':True});
	time.sleep(3)

