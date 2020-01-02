from flask import Flask, flash, redirect, render_template, request, session, abort
import socket
import _thread
import time
import os
import subprocess
import configparser

app = Flask(__name__)

statusControl = 0
dataCollStatus = 0;
theStatus = "Not yet started"

# this thread can be scheduled for DE heartbeat check
def check_status(threadName, delay):
   print("Enter check_status")
   statusControl = 1;
   while (statusControl == 1):
      print("Status inquiry to LH")
      theCommand = 'S?'
      host_ip, server_port = "127.0.0.1", 6100
      data = theCommand + "\n"
   
    # Initialize a TCP client socket using SOCK_STREAM
      tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

      try:
    # Establish connection to TCP server and exchange data
        tcp_client.connect((host_ip, server_port))
        tcp_client.sendall(data.encode())

    # Read data from the TCP server and close the connection
        received = tcp_client.recv(1024)
        print("LH answered ", received, " substr = '", received[0:2].decode("ASCII"), "'")
        if(received[0:2].decode("ASCII") == "OK"):
          print("status is ON")
          theStatus = "ON"
      except Exception as e: 
        print(e)
        return
      finally:
        tcp_client.close() 
      print("Thread sleep")     
      time.sleep(delay)
      print("exit sleep")

def check_status_once():
  print(" *********** Status inquiry to LH *********")
  global theStatus
  theStatus = "DE is off or disconnected (did discovery run??)"
  theCommand = 'S?'
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("*** WC: *** connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("send query")
     tcp_client.sendall(data.encode())
     print("wait")
     time.sleep(1)
     print("try to receive response")
    # Read data from the TCP server and close the connection
     try:

# we should receive a NAK from mainctl if mainctl is running but DE unresponsive
# 12/12/19 - mainctl tries to send NAK but we never get it

       received = tcp_client.recv(1024, socket.MSG_DONTWAIT)
     except Exception as e:
       print("exception on recv")
       theStatus = "Mainctl stopped or DE disconnected , error: " + str(e)
     print("LH answered ", received, " substr = '", received[0:2].decode("ASCII"), "'")
     if(received[0:2].decode("ASCII") == "OK"):
       print("status is ON")
       theStatus = "ON"
  except Exception as e: 
     print(e)
     print("'" + e.errno + "'")
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error " + e.errno +  "mainctl program not responding"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     return(theStatus)

theStatus = "Off"

@app.route("/desetup2",methods=['POST','GET'])
def members():
   return render_template('desetup.html')
   return "Members"


# Here is the home page
@app.route("/")
def sdr():
   global theStatus;
   theStatus = check_status_once()
   print("WEB status ", theStatus)
   return render_template('tangerine.html',result = theStatus)


@app.route("/restart")
def restart():
   global theStatus
   print("restart")
   returned_value = os.system("killall -9 main")
   print("after killing mainctl, retcode=",returned_value)
   print("Trying to restart mainctl")
   returned_value = subprocess.Popen("/home/odroid/projects/TangerineSDR-notes/mainctl/main")
   time.sleep(3)
   print("after restarting mainctl, retcode=",returned_value)
   stopcoll()
   theStatus = check_status_once()
   return redirect('/')

   
@app.route("/config",methods=['POST','GET'])
def config():
   parser = configparser.ConfigParser()
   parser.read('config.ini')
   if request.method == 'POST':
     result = request.form
     print("result of config post =")
     print(result.get('theToken'))
     parser.set('profile', 'token_value', result.get('theToken'))
     parser.set('profile', 'latitude',    result.get('theLatitude'))
     parser.set('profile', 'longitude',   result.get('theLongitude'))
     parser.set('profile', 'elevation',   result.get('theElevation'))
     
     fp = open('config.ini','w')
     parser.write(fp)
     fp.close()

   theToken =     parser['profile']['token_value']
   theLatitude =  parser['profile']['latitude']
   theLongitude = parser['profile']['longitude']
   theElevation = parser['profile']['elevation']
   print("token = " + theToken)
   return render_template('config.html', theToken = theToken,
     theLatitude = theLatitude, theLongitude = theLongitude,
     theElevation = theElevation )

@app.route("/clocksetup", methods = ['POST','GET'])
def clocksetup():
   return render_template('clock.html')

@app.route("/channelantennasetup", methods = ['POST','GET'])
def channelantennasetup():
	return render_template('channelantennasetup.html')

@app.route("/desetup",methods=['POST','GET'])
def desetup():
   print("reached DE setup")
   parser = configparser.ConfigParser()
   parser.read('config.ini')
   if request.method == 'GET':
     ringbufferPath = parser['settings']['ringbuffer_path']
     DEIP           = parser['settings']['de_ip']
     ant0 =     parser['settings']['ant0']
     ch0f =     parser['settings']['ch0f']
     ch0b =     parser['settings']['ch0b']     
     ant1 =     parser['settings']['ant1']
     ch1f =     parser['settings']['ch1f']
     ch1b =     parser['settings']['ch1b']
     ant2 =     parser['settings']['ant2']
     ch2f =     parser['settings']['ch2f']
     ch2b =     parser['settings']['ch2b']
     ant3 =     parser['settings']['ant3']
     ch3f =     parser['settings']['ch3f']
     ch3b =     parser['settings']['ch3b']
     print("ringbufferPath=",ringbufferPath)
     return render_template('desetup.html',
	  ringbufferPath = ringbufferPath,
      ant0 = ant0 , ch0f = ch0f, ch0b = ch0b,
	  ant1 = ant1 , ch1f = ch1f, ch1b = ch1b,
      ant2 = ant2 , ch2f = ch2f, ch2b = ch2b,
	  ant3 = ant3,  ch3f = ch3f, ch3b = ch3b,
      DEIP = DEIP)

   if request.method == 'POST':
     result = request.form
     print("result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("CANCEL")
     else:
       print("result of DEsetup post =")
       ringbufferPath = ""
       DEIP = ""
       parser.set('settings', 'ringbuffer_path', result.get('ringbufferPath'))
       parser.set('settings', 'DE_IP',           result.get('DEIP'))
       parser.set('settings', 'ant0',            str(result.get('ch0a')))
       parser.set('settings', 'ch0f',            str(result.get('ch0f')))
       parser.set('settings', 'ch0b',            str(result.get('ch0b')))
       parser.set('settings', 'ant1',            str(result.get('ch1a')))
       parser.set('settings', 'ch1f',            str(result.get('ch1f')))
       parser.set('settings', 'ch1b',            str(result.get('ch1b')))
       parser.set('settings', 'ant2',            str(result.get('ch2a')))
       parser.set('settings', 'ch2f',            str(result.get('ch2f')))
       parser.set('settings', 'ch2b',            str(result.get('ch2b')))
       parser.set('settings', 'ant3',            str(result.get('ch3a')))
       parser.set('settings', 'ch3f',            str(result.get('ch3f')))
       parser.set('settings', 'ch3b',            str(result.get('ch3b')))
     
       fp = open('config.ini','w')
       parser.write(fp)
       fp.close()

   ringbufferPath = parser['settings']['ringbuffer_path']
   DEIP           = parser['settings']['de_ip']
   ant0 =     parser['settings']['ant0']
   ch0f =     parser['settings']['ch0f']
   ch0b =     parser['settings']['ch0b']     
   ant1 =     parser['settings']['ant1']
   ch1f =     parser['settings']['ch1f']
   ch1b =     parser['settings']['ch1b']
   ant2 =     parser['settings']['ant2']
   ch2f =     parser['settings']['ch2f']
   ch2b =     parser['settings']['ch2b']
   ant3 =     parser['settings']['ant3']
   ch3f =     parser['settings']['ch3f']
   ch3b =     parser['settings']['ch3b']
   return render_template('desetup.html',
	ringbufferPath = ringbufferPath,
    ant0 = ant0 , ch0f = ch0f, ch0b = ch0b,
	ant1 = ant1 , ch1f = ch1f, ch1b = ch1b,
    ant2 = ant2 , ch2f = ch2f, ch2b = ch2b,
	ant3 = ant3,  ch3f = ch3f, ch3b = ch3b,
    DEIP = DEIP)

@app.route("/startcollection")
def startcoll():
  global theStatus
  print("Start Data Collection command")
  
  theCommand = 'SC'
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("send command")
     tcp_client.sendall(data.encode())
  except Exception as e: 
     print(e)
     print("'" + e.errno + "'")
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error " + e.errno +  "mainctl program not responding"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     theDataStatus = "Started data collection"
     dataCollStatus = 1
     return render_template('tangerine.html', result = theStatus, dataStat = theDataStatus)
  return

@app.route("/stopcollection")
def stopcoll():
  global theStatus
  print("Stop Data Collection command")
  theCommand = 'XC'
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("send command")
     tcp_client.sendall(data.encode())
  except Exception as e: 
     print(e)
     print("'" + e.errno + "'")
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error " + e.errno +  "mainctl program not responding"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     theDataStatus = "Stopped data collection"
     dataCollStat = 0
     return render_template('tangerine.html', dataStat = theDataStatus)
  return




if __name__ == "__main__":
#	app.run(host='0.0.0.0')
#	app.run(debug = True)

	from waitress import serve
#	serve(app, host = "0.0.0.0", port=5000) 
	serve(app)
#	serve(app, host = "192.168.1.75", port=5000)

