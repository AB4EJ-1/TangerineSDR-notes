from flask import Flask, flash, redirect, render_template, request, session, abort
import socket
import _thread
import time
import os
import subprocess
import configparser

from flask_wtf import Form
# following is for future flask upgrade
#from FlaskForm import Form
from wtforms import TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField
from flask import request, flash
from forms import MainControlForm, ThrottleControlForm
#from forms import ContactForm

from wtforms import validators, ValidationError

app = Flask(__name__)
app.secret_key = 'development key'
global theStatus, theDataStatus
statusControl = 0
dataCollStatus = 0;
theStatus = "Not yet started"
theDataStatus = ""


def check_status_once():
  print(" *********** Status inquiry to LH *********")
  global theStatus, theDataStatus
  theStatus = "DE is off or disconnected, or mainctl stopped"
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
     time.sleep(3)
     print("try to receive response")
     received = "NOTHING"
    # Read data from the TCP server and close the connection
     try:

# we should receive a NAK from mainctl if mainctl is running but DE unresponsive
# 12/12/19 - mainctl tries to send NAK but we never get it

       received = tcp_client.recv(1024, socket.MSG_DONTWAIT)
       print("received data from DE: ", received)
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

theStatus = "Off or not connected. Needs restart."

@app.route("/desetup2",methods=['POST','GET'])
def members():
   return render_template('desetup.html')


# Here is the home page
@app.route("/", methods = ['GET', 'POST'])
def sdr():
   form = MainControlForm()
   global theStatus, theDataStatus

   if request.method == 'GET':  

     form.destatus = theStatus
     form.dataStat = theDataStatus
     print("home page, status = ",theStatus)
     return render_template('tangerine.html',form = form)

   if request.method == 'POST':
      print("Main control POST")
      if form.validate() == False:
         flash('All fields are required.')
         return render_template('tangerine.html', form = form)
      else:
         result = request.form
         print('mode set to:',form.mode.data)
         print('start set to ',form.startDC.data)
         print('stop set to ', form.stopDC.data)
         if(form.startDC.data ):
            startcoll()
         if(form.stopDC.data ):
            stopcoll()
         print("end of control loop; theStatus=", theStatus)
         form.destatus = theStatus
         form.dataStat = theDataStatus
         return render_template('tangerine.html', form = form)


@app.route("/restart")
def restart():
   global theStatus, theDataStatus
   print("restart")
   returned_value = os.system("killall -9 main")
   print("after killing mainctl, retcode=",returned_value)
   print("Trying to restart mainctl")
   returned_value = subprocess.Popen("/home/odroid/projects/TangerineSDR-notes/mainctl/mainctl")
   time.sleep(3)
   print("after restarting mainctl, retcode=",returned_value)
#   stopcoll()
   theStatus = check_status_once()
   return redirect('/')


@app.route("/chkstat")
def chkstat():
   global theStatus, theDatastatus
   theStatus = check_status_once();
   return redirect('/')

   
@app.route("/config",methods=['POST','GET'])
def config():
   global theStatus, theDataStatus
   parser = configparser.ConfigParser(allow_no_value=True)
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
   global theStatus, theDataStatus
   return render_template('clock.html')

@app.route("/channelantennasetup", methods = ['POST','GET'])
def channelantennasetup():
   global theStatus, theDataStatus
   return render_template('channelantennasetup.html')

@app.route("/desetup",methods=['POST','GET'])
def desetup():
   global theStatus, theDataStatus
   print("reached DE setup")
   parser = configparser.ConfigParser(allow_no_value=True)
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
  form = MainControlForm()
  global theStatus, theDataStatus
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
     form.dataStat = theDataStatus

  return 

@app.route("/stopcollection")
def stopcoll():
  form = MainControlForm()
  global theStatus, theDataStatus
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
#     print("'" + e.errno + "'")
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error " + str(e.errno) +  "mainctl program not responding"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     theDataStatus = "Stopped data collection"
     dataCollStat = 0

  return


@app.route("/throttle", methods = ['POST','GET'])
def throttle():
   global theStatus, theDataStatus
   form = ThrottleControlForm()
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   if request.method == 'GET':
     throttle = parser['settings']['throttle']
     return render_template('throttle.html',
	  throttle = throttle, form = form)

   if request.method == 'POST':
     result = request.form
     print("result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("CANCEL")
     else:
       print("result of throttle post =")
       throttle= ""
       parser.set('settings', 'throttle', result.get('throttle'))
       fp = open('config.ini','w')
       parser.write(fp)
       fp.close()

   ringbufferPath = parser['settings']['throttle']

   return render_template('throttle.html', throttle = throttle, form = form)

@app.route("/callsign", methods = ['POST','GET'])
def callsign():
   global theStatus, theDataStatus
#   form = CallsignForm()
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   if request.method == 'GET':
     c0 = parser['monitor']['c0']
     c1 = parser['monitor']['c1']
     c2 = parser['monitor']['c2']
     c3 = parser['monitor']['c3']
     c4 = parser['monitor']['c4']
     c5 = parser['monitor']['c5']
     return render_template('callsign.html',
	  c0 = c0, c1 = c1, c2 = c2, c3 = c3, c4 = c4, c5 = c5)
   if request.method == 'POST':
     result = request.form
     print("result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("CANCEL")
     else:
       print("result of callsign post =")
       ringbufferPath = ""
       DEIP = ""
       parser.set('monitor', 'c0', result.get('c0'))
       parser.set('monitor', 'c1', result.get('c1'))
       parser.set('monitor', 'c2', result.get('c2'))
       parser.set('monitor', 'c3', result.get('c3'))
       parser.set('monitor', 'c4', result.get('c4'))
       parser.set('monitor', 'c5', result.get('c5'))
       fp = open('config.ini','w')
       parser.write(fp)

     c0 = parser['monitor']['c0']
     c1 = parser['monitor']['c1']
     c2 = parser['monitor']['c2']
     c3 = parser['monitor']['c3']
     c4 = parser['monitor']['c4']
     c5 = parser['monitor']['c5']
     
     return render_template('callsign.html',
	  c0 = c0, c1 = c1, c2 = c2, c3 = c3, c4 = c4, c5 = c5)

@app.route("/notification", methods = ['POST','GET'])
def notification():
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   if request.method == 'GET':
     print("smtpsvr = ", parser['email']['smtpsvr'])
     smtpsvr =     parser['email']['smtpsvr']
     emailfrom=    parser['email']['emailfrom']
     emailto =     parser['email']['emailto']
     smtpport =    parser['email']['smtpport']
     smtptimeout = parser['email']['smtptimeout']
     smtpuid =     parser['email']['smtpuid']
     smtppw =      parser['email']['smtppw']
     return render_template('notification.html',
	  smtpsvr = smtpsvr, emailfrom = emailfrom,
      emailto = emailto, smtpport = smtpport,
      smtptimeout = smtptimeout, smtpuid = smtpuid,
      smtppw = smtppw)

   if request.method == 'POST':
     result = request.form
     print("result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("CANCEL")

     else:
        print("reached POST on notification;", result.get('smtpsvr'))
        parser.set('email', 'smtpsvr', result.get('smtpsvr'))
        parser.set('email', 'emailfrom', result.get('emailfrom'))
        parser.set('email', 'emailto', result.get('emailto'))
        parser.set('email', 'smtpport', result.get('smtpport'))
        parser.set('email', 'smtptimeout', result.get('smtptimeout'))
        parser.set('email', 'smtpuid', result.get('smtpuid'))
        parser.set('email', 'smtppw', result.get('smtppw'))
        fp = open('config.ini','w')
        parser.write(fp)

     smtpsvr =     parser['email']['smtpsvr']
     emailfrom=    parser['email']['emailfrom']
     emailto =     parser['email']['emailto']
     smtpport =    parser['email']['smtpport']
     smtptimeout = parser['email']['smtptimeout']
     smtpuid =     parser['email']['smtpuid']
     smtppw =      parser['email']['smtppw']
     return render_template('notification.html',
	  smtpsvr = smtpsvr, emailfrom = emailfrom,
      emailto = emailto, smtpport = smtpport,
      smtptimeout = smtptimeout, smtpuid = smtpuid,
      smtppw = smtppw)



@app.errorhandler(404)
def page_not_found(e):
    return render_template("notfound.html")


if __name__ == "__main__":
#	app.run(host='0.0.0.0')
#	app.run(debug = True)

	from waitress import serve
#	serve(app, host = "0.0.0.0", port=5000) 
	serve(app)
#	serve(app, host = "192.168.1.75", port=5000)

