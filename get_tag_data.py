'''
Print sensor data to the screen. Update interval 2 sec.

Press Ctrl+C to quit.

2017-02-02 13:45:25.233400
Sensor - F4:9D:33:83:30:29
Temperature: 10
Humidity:    28
Pressure:    689
'''

'''
get sensor data from ruuvitag and send data to Netpie, ThingSpeak

'''

import sys
#from urllib.request import urlopen
import urllib.request
import json
import paho.mqtt.client as mqtt
import time
import os
from datetime import datetime

from ruuvitag_sensor.ruuvitag import RuuviTag

import microgear.client as microgear
import logging

CLEAN_SESSION=True
broker="mqtt.ogonan.com"

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
microgear.connect(False)

# Enter Your API key here
myAPI = 'N7T61S58Z0GX7S8Z'
# URL where we will send the data, Don't change it
baseURL = 'https://api.thingspeak.com/update?api_key=%s' % myAPI

writeAPIkey = "N7T61S58Z0GX7S8Z" # Replace YOUR-CHANNEL-WRITEAPIKEY with your channel write API key
channelID = "793984" # Replace YOUR-CHANNELID with your channel ID
#url = "https://api.thingspeak.com/channels/"+channelID+"/bulk_update.json" # ThingSpeak server settings
url = "https://api.thingspeak.com/update?api_key=%s" % writeAPIkey
messageBuffer = []
fields = ''
_temperature = 19.0

lastConnectionTime = time.time() # Track the last connection time
lastUpdateTime = time.time() # Track the last update time
postingInterval = 60 # Post data once every 1 minutes
updateInterval = 15 # Update once every 15 seconds

# Change here your own device's mac-address ruuvitag
mac = 'F4:9D:33:83:30:29'

print('Starting')

sensor = RuuviTag(mac)

def on_disconnect(client, userdata, flags, rc=0):
    m="DisConnected flags"+"result code "+str(rc)
    print(m)

def on_connect(client, userdata, flags, rc):
    print("Connected flags ",str(flags),"result code ",str(rc))

def on_message(client, userdata, message):
    print("message received  "  ,str(message.payload.decode("utf-8")))
def on_publish(client, userdata, mid):
    print("message published "  ,str(message.payload.decode("utf-8")))

def mqttConnect():
    print("creating client 1 with clean session set to",CLEAN_SESSION)
    client1 = mqtt.Client("Python1",clean_session=CLEAN_SESSION)    #create new instance
    ## edit code for passwords
    print("setting  password")
    client1.username_pw_set(username="kaebmoo",password="sealwiththekiss")
    client1.on_message=on_message        #attach function to callback
    client1.on_connect=on_connect
    print("connecting to ",broker)
    client1.connect(broker)

    client1.loop_start()
    #client1.on_disconnect=on_disconnect
    time.sleep(3)
    #client1.loop()
    
    client1.disconnect()
    client1.loop_stop()

def httpRequest():
    '''Function to send the POST request to
    ThingSpeak channel for bulk update.'''

    # global messageBuffer
    global url
    global fields
    global _temperature

    # data = json.dumps({'write_api_key':writeAPIkey,'updates':messageBuffer}) # Format the json data buffer
    url_req = url + fields
    # print(url_req)
    req = urllib.request.Request(url = url_req)

    # requestHeaders = {"User-Agent":"mw.doc.bulk-update (Raspberry Pi)","Content-Type":"application/json","Content-Length":str(len(data))}
    # for key,val in requestHeaders.items(): # Set the headers
    #   req.add_header(key,val)
    ## req.add_data(data) # Add the data to the request
    # req.data = data.encode("utf-8")
    # print(req.data)

    # Make the request to ThingSpeak
    try:
        # response = urllib.request.urlopen(req) # Make the request
        # print(response.getcode()) # A 202 indicates that the server has accepted the request

        print('sending data to netpie\n')
        print('/brew/temperature\n')
        microgear.publish("/brew/temperature",_temperature,{'retain':True});
    # except urllib.request.HTTPError as e:
    except urllib.request.URLError as e:
        if hasattr(e, 'reason'):
            print('We failed to reach a server.')
            print('Reason: ', e.reason)
        elif hasattr(e, 'code'):
            print('The server couldn\'t fulfill the request.')
            print('Error code: ', e.code)
    else:
        # everything is fine
        # print(e.code) # Print the error code
        # messageBuffer = [] # Reinitialize the message buffer

        global lastConnectionTime
        lastConnectionTime = time.time() # Update the connection time

#while True:
def ruuvitag_data():
    data = sensor.update()

    try:
        sensor_node = str.format('{0}', mac)
        temperature = str.format('{0}', data['temperature'])
        humidity = str.format('{0}', data['humidity'])
        pressure = str.format('{0}', data['pressure'])
        battery = str.format('{0}', data['battery'])
    except KeyError:
        print('Can not find "something"')
        temperature = _temperature
        if 'humidity' or 'pressure' or 'battery' not in data:
            humidity = str(0)
            pressure = str(0)
            battery = str(0)
        else:
            pass

    return sensor_node, temperature, humidity, pressure, battery

def updatesJson():
    '''Function to update the message buffer
    every 15 seconds with data. And then call the httpRequest
    function every 2 minutes. This examples uses the relative timestamp as it uses the "delta_t" parameter.
    If your device has a real-time clock, you can also provide the absolute timestamp using the "created_at" parameter.
    '''

    global lastUpdateTime
    global fields
    fields = ''
    # message = {}
    # message['delta_t'] = int(round(time.time() - lastUpdateTime))
    try:
        sensor_node, temperature, humidity, pressure, battery = ruuvitag_data()
        fields = '&field1=%s' % temperature + '&field2=%s' % humidity + '&field3=%s' % battery
        print(fields)
    except KeyError:
        print('Can not find "something" in updatesJson')
        temperature = _temperature

    # message['field1'] = temperature
    # message['field2'] = humidity
    # message['field3'] = battery
    # global messageBuffer
    #messageBuffer.append(message)

    # If posting interval time has crossed 2 minutes update the ThingSpeak channel with your data
    if time.time() - lastConnectionTime >= postingInterval:
        httpRequest()
    lastUpdateTime = time.time()

while True:
    # Wait for 15 seconds and start over again
    try:
        #sensor_node, temperature, humidity, pressure, battery = ruuvitag_data()
        if time.time() - lastUpdateTime >= updateInterval:
            updatesJson()

        try:
            sensor_node, temperature, humidity, pressure, battery = ruuvitag_data()
            _temperature = temperature
        except KeyError:
            print('Can not find "something" in while loop')

        # Clear screen and print sensor data
        # os.system('clear')
        print('Press Ctrl+C to quit.\n\r\n\r')
        print(str(datetime.now()))
        print('sensor node: ', sensor_node)
        print('temperature: ', temperature,  ' C')
        print('humidity:    ', humidity,  ' %')
        print('pressure:    ', pressure)
        print('battery:     ', battery)
        print('\n\r\n\r.......')


        time.sleep(15)
    except KeyboardInterrupt:
        # When Ctrl+C is pressed execution of the while loop is stopped
        print('Exit')
        break

