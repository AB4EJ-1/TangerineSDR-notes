from flask import Flask, flash, redirect, render_template, request, session, abort
import socket
import _thread
import time
import os
import subprocess
import configparser

app = Flask(__name__)

#@app.route("/")
#def index():
#    return "Index!"

statusControl = 0

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
  print("Status inquiry to LH")
  theStatus = "DE is off or disconnected"
  theCommand = 'S?'
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("connect to socket")
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
@app.route("/hello")
def hello():
    return "Hello World!"

@app.route("/members")
def members():
    return "Members"

@app.route("/hello1/<thename>/")
def hello1(thename):
    return render_template(
    'tangerine.html',name=thename)

@app.route("/members/<string:name>/")
def getMember(name):
	return name
#    return name</string:name>


# Here is the home page
@app.route("/")
def sdr():
   theStatus = check_status_once()
   print("WEB status ", theStatus)
   return render_template('tangerine.html',result = theStatus)


@app.route("/restart")
def restart():
   print("restart")
   returned_value = os.system("killall -9 main")
   print("after killing mainctl, retcode=",returned_value)
   print("Trying to restart mainctl")
   returned_value = subprocess.Popen("/home/odroid/projects/TangerineSDR-notes/mainctl/main")
   print("after restarting mainctl, retcode=",returned_value)
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
     parser.set('profile', 'latitude', result.get('theLatitude'))
     parser.set('profile', 'longitude', result.get('theLongitude'))
     parser.set('profile', 'elevation', result.get('theElevation'))
     
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

@app.route('/student')
def student():
   return render_template('student.html')

@app.route('/result',methods = ['POST', 'GET'])
def result():
   if request.method == 'POST':
      result = request.form
      print("result -")
      print(result.get('Name'))
      theCommand = result.get('Name')
      host_ip, server_port = "127.0.0.1", 6100
      data = theCommand + "\n"

# Initialize a TCP client socket using SOCK_STREAM
      tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

      try:
    # Establish connection to TCP server and exchange data
        tcp_client.connect((host_ip, server_port))
        tcp_client.sendall(data.encode())

    # Read data from the TCP server and close the connection
#    received = tcp_client.recv(1024)
      finally:
        tcp_client.close()

      print ("Bytes Sent:     {}".format(data))
#      print('"',theCommand,"'")
      return render_template("result.html",result = result)


if __name__ == "__main__":
	app.run(debug = True)
# uncomment following lines to run production server "wairess"
	from waitress import serve
	serve(app, host = "0.0.0.0", port=5000)
