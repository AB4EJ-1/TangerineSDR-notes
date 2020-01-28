from flask import Flask, flash, redirect, render_template, request, session, abort
import socket
import _thread
import time
import os
import subprocess
import configparser
import smtplib
from email.mime.multipart import MIMEMultipart 
from email.mime.text import MIMEText 
from extensions import csrf
from config import Config
from flask_wtf import Form
# following is for future flask upgrade
#from FlaskForm import Form
from wtforms import TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField
from flask import request, flash
from forms import MainControlForm, ThrottleControlForm, ChannelControlForm, ServerControlForm
#from forms import ContactForm

from wtforms import validators, ValidationError
from flask_wtf import CSRFProtect

app = Flask(__name__)
#app.config['SECRET_KEY'] = 'this-is-really-secret'
#app.secret_key = 'development key'
app.config.from_object(Config)
csrf.init_app(app)
#CsrfProtect(app)
global theStatus, theDataStatus, thePropStatus
statusControl = 0
dataCollStatus = 0;
theStatus = "Not yet started"
theDataStatus = ""
thePropStatus = 0


def check_status_once():
  print("F: *********** Status inquiry to LH *********")
  global theStatus, theDataStatus
  theStatus = "DE is off or disconnected, or mainctl stopped"
  theCommand = 'S?'
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("F: *** WC: *** connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("F: send query")
     tcp_client.sendall(data.encode())
     print("F: wait for DE response")
     time.sleep(4)
     print("F: try to receive response")
     received = "NOTHING"
    # Read data from the TCP server and close the connection
     try:
       received = tcp_client.recv(1024, socket.MSG_DONTWAIT)
       print("F: received data from DE: ", received)
     except Exception as e:
       print("F: exception on recv")
       theStatus = "Mainctl stopped or DE disconnected , error: " + str(e)
     print("F: LH answered ", received, " substr = '", received[0:2].decode("ASCII"), "'")
     if(received[0:2].decode("ASCII") == "OK"):
       print("F: status is ON")
       theStatus = "ON"
  except Exception as e: 
     print(e)
     print("F: '" + e.errno + "'")
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error " + e.errno +  "mainctl program not responding"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     return(theStatus)

  theStatus = "Off or not connected. Needs restart."

#@app.route("/desetup2",methods=['POST','GET'])
#def members():
#   return render_template('desetup.html')

#####################################################################
# Here is the home page
@app.route("/", methods = ['GET', 'POST'])
def sdr():
   form = MainControlForm()
   global theStatus, theDataStatus
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')

   if request.method == 'GET':  
     form.mode.data = parser['settings']['mode']
     form.destatus = theStatus
     form.dataStat = theDataStatus
     print("F: home page, status = ",theStatus)
     return render_template('tangerine.html',form = form)

   if request.method == 'POST':
      print("F: Main control POST; mode set to ",form.mode.data)
      form.errline = ""
      if form.validate() == False:
         flash('All fields are required.')
         return render_template('tangerine.html', form = form)
      else:
         result = request.form
         print('F: mode set to:',form.mode.data)
         parser.set('settings','mode',form.mode.data)
         fp = open('config.ini','w')
         parser.write(fp)
         fp.close()
         print('F: start set to ',form.startDC.data)
         print('F: stop set to ', form.stopDC.data)
         if(form.startDC.data ):
            if ( len(parser['settings']['ringbuffer_path']) < 1 
                   and form.mode.data == 'ringbuffer') :
              print("F: configured ringbuffer path='", parser['settings']['ringbuffer_path'],"'", len(parser['settings']['ringbuffer_path']))
              form.errline = 'ERROR: Path to digital data storage not configured'
            else:
              startcoll()
         if(form.stopDC.data ):
            stopcoll()
         if(form.startprop.data):
            startprop()
         if(form.stopprop.data) :
            stopprop()
         print("F: end of control loop; theStatus=", theStatus)
         form.destatus = theStatus
         form.dataStat = theDataStatus
         return render_template('tangerine.html', form = form)


@app.route("/restart1")
def restart():
   global theStatus, theDataStatus
   print("F: restart")
   returned_value = os.system("killall -9 mainctl")
   print("F: after killing mainctl, retcode=",returned_value)
   print("F: Trying to restart mainctl")
   returned_value = subprocess.Popen("/home/odroid/projects/TangerineSDR-notes/mainctl/mainctl")
   time.sleep(3)
   print("F: after restarting mainctl, retcode=",returned_value)
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
     print("F: result of config post =")
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
   print("F: token = " + theToken)
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
   form = ChannelControlForm()
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   theStatus = ""
   if request.method == 'GET':
     ringbufferPath = parser['settings']['ringbuffer_path']
     form.antennaport0.data =     parser['settings']['ant0']
     form.antennaport1.data =     parser['settings']['ant1']
     form.antennaport2.data =     parser['settings']['ant2']
     form.antennaport3.data =     parser['settings']['ant3']
     form.antennaport4.data =     parser['settings']['ant4']
     form.antennaport5.data =     parser['settings']['ant5']
     form.antennaport6.data =     parser['settings']['ant6']
     form.antennaport7.data =     parser['settings']['ant7']
     form.antennaport8.data =     parser['settings']['ant8']
     form.antennaport9.data =     parser['settings']['ant8']
     form.antennaport10.data =     parser['settings']['ant10']
     form.antennaport11.data =     parser['settings']['ant11']
     form.antennaport12.data =     parser['settings']['ant12']
     form.antennaport13.data =     parser['settings']['ant13']
     form.antennaport14.data =     parser['settings']['ant14']
     form.antennaport15.data =     parser['settings']['ant15']
     ch0f =     parser['settings']['ch0f']
     ch0b =     parser['settings']['ch0b']     
     ch1f =     parser['settings']['ch1f']
     ch1b =     parser['settings']['ch1b']
     ch2f =     parser['settings']['ch2f']
     ch2b =     parser['settings']['ch2b']
     ch3f =     parser['settings']['ch3f']
     ch3b =     parser['settings']['ch3b']
     ch4f =     parser['settings']['ch4f']
     print("ch4f='"+ch4f+"'")
     ch4b =     parser['settings']['ch4b']
     ch5f =     parser['settings']['ch5f']
     ch5b =     parser['settings']['ch5b']
     ch6f =     parser['settings']['ch6f']
     ch6b =     parser['settings']['ch6b']
     ch7f =     parser['settings']['ch7f']
     ch7b =     parser['settings']['ch7b']
     ch8f =     parser['settings']['ch8f']
     ch8b =     parser['settings']['ch8b']
     ch9f =     parser['settings']['ch9f']
     ch9b =     parser['settings']['ch9b']
     ch10f =     parser['settings']['ch10f']
     ch10b =     parser['settings']['ch10b']
     ch11f =     parser['settings']['ch11f']
     ch11b =     parser['settings']['ch11b']
     ch12f =     parser['settings']['ch12f']
     ch12b =     parser['settings']['ch12b']
     ch13f =     parser['settings']['ch13f']
     ch13b =     parser['settings']['ch13b']
     ch14f =     parser['settings']['ch14f']
     ch14b =     parser['settings']['ch14b']
     ch15f =     parser['settings']['ch15f']
     ch15b =     parser['settings']['ch15b']
     print("F: ringbufferPath=",ringbufferPath)
     return render_template('desetup.html',
      form = form, status = theStatus,
	  ringbufferPath = ringbufferPath,
      ch0f = ch0f, ch0b = ch0b,
	  ch1f = ch1f, ch1b = ch1b,
      ch2f = ch2f, ch2b = ch2b,
	  ch3f = ch3f, ch3b = ch3b,
	  ch4f = ch4f, ch4b = ch4b,
	  ch5f = ch5f, ch5b = ch5b,
	  ch6f = ch6f, ch6b = ch6b,
	  ch7f = ch7f, ch7b = ch7b,
	  ch8f = ch8f, ch8b = ch8b,
	  ch9f = ch9f, ch9b = ch9b,
	  ch10f = ch10f, ch10b = ch10b,
	  ch11f = ch11f, ch11b = ch11b,
	  ch12f = ch12f, ch12b = ch12b,
	  ch13f = ch13f, ch13b = ch13b,
	  ch14f = ch14f, ch14b = ch14b,
	  ch15f = ch15f, ch15b = ch15b )

   if not form.validate():
     theStatus = form.errors
#     ringbufferPath = result.get('ringbufferPath'))
     result = request.form
     ringbufferPath = result.get('ringbufferPath')
     ch0f =     str(result.get('ch0f'))
     ch0b =     str(result.get('ch0b'))     
     ch1f =     str(result.get('ch1f'))
     ch1b =     str(result.get('ch1b'))
     ch2f =     str(result.get('ch2f'))
     ch2b =     str(result.get('ch2b'))
     ch3f =     str(result.get('ch3f'))
     ch3b =     str(result.get('ch3b'))
     ch4f =     str(result.get('ch4f'))
     ch4b =     str(result.get('ch4b'))
     ch5f =     str(result.get('ch5f'))
     ch5b =     str(result.get('ch5b'))
     ch6f =     str(result.get('ch6f'))
     ch6b =     str(result.get('ch6b'))
     ch7f =     str(result.get('ch7f'))
     ch7b =     str(result.get('ch7b'))
     ch8f =     str(result.get('ch8f'))
     ch8b =     str(result.get('ch8b'))
     ch9f =     str(result.get('ch9f'))
     ch9b =     str(result.get('ch9b'))
     ch10f =    str(result.get('ch10f'))
     ch10b =    str(result.get('ch10b'))
     ch11f =    str(result.get('ch11f'))
     ch11b =    str(result.get('ch11b'))
     ch12f =    str(result.get('ch12f'))
     ch12b =    str(result.get('ch12b'))
     ch13f =    str(result.get('ch13f'))
     ch13b =    str(result.get('ch13b'))
     ch14f =    str(result.get('ch14f'))
     ch14b =    str(result.get('ch14b'))
     ch15f =    str(result.get('ch15f'))
     ch15b =    str(result.get('ch15b'))
     return render_template('desetup.html',
	  ringbufferPath = ringbufferPath,
      form = form, status = theStatus,
      ch0f = ch0f, ch0b = ch0b,
	  ch1f = ch1f, ch1b = ch1b,
      ch2f = ch2f, ch2b = ch2b,
	  ch3f = ch3f, ch3b = ch3b,
	  ch4f = ch4f, ch4b = ch4b,
	  ch5f = ch5f, ch5b = ch5b,
	  ch6f = ch6f, ch6b = ch6b,
	  ch7f = ch7f, ch7b = ch7b,
	  ch8f = ch8f, ch8b = ch8b,
	  ch9f = ch9f, ch9b = ch9b,
	  ch10f = ch10f, ch10b = ch10b,
	  ch11f = ch11f, ch11b = ch11b,
	  ch12f = ch12f, ch12b = ch12b,
	  ch13f = ch13f, ch13b = ch13b,
	  ch14f = ch14f, ch14b = ch14b,
	  ch15f = ch15f, ch15b = ch15b )
   
   if request.method == 'POST' and form.validate() :
     result = request.form
     print("F: result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("F: CANCEL")
     else:
       print("F: POST ringbufferPath =", result.get('ringbufferPath'))
       ringbufferPath = ""
       parser.set('settings', 'ringbuffer_path', result.get('ringbufferPath'))
       parser.set('settings', 'ant0',            form.antennaport0.data)
       parser.set('settings', 'ch0f',            str(result.get('ch0f')))
       parser.set('settings', 'ch0b',            str(result.get('ch0b')))
       parser.set('settings', 'ant1',            form.antennaport1.data)
       parser.set('settings', 'ch1f',            str(result.get('ch1f')))
       parser.set('settings', 'ch1b',            str(result.get('ch1b')))
       parser.set('settings', 'ant2',            form.antennaport2.data)
       parser.set('settings', 'ch2f',            str(result.get('ch2f')))
       parser.set('settings', 'ch2b',            str(result.get('ch2b')))
       parser.set('settings', 'ant3',            form.antennaport3.data)
       parser.set('settings', 'ch3f',            str(result.get('ch3f')))
       parser.set('settings', 'ch3b',            str(result.get('ch3b')))
       parser.set('settings', 'ant4',            form.antennaport4.data)
       parser.set('settings', 'ch4f',            str(result.get('ch4f')))
       parser.set('settings', 'ch4b',            str(result.get('ch4b')))
       parser.set('settings', 'ant5',            form.antennaport5.data)
       parser.set('settings', 'ch5f',            str(result.get('ch5f')))
       parser.set('settings', 'ch5b',            str(result.get('ch5b')))
       parser.set('settings', 'ant6',            form.antennaport6.data)
       parser.set('settings', 'ch6f',            str(result.get('ch6f')))
       parser.set('settings', 'ch6b',            str(result.get('ch6b')))
       parser.set('settings', 'ant7',            form.antennaport7.data)
       parser.set('settings', 'ch7f',            str(result.get('ch7f')))
       parser.set('settings', 'ch7b',            str(result.get('ch7b')))
       parser.set('settings', 'ant8',            form.antennaport7.data)
       parser.set('settings', 'ch8f',            str(result.get('ch8f')))
       parser.set('settings', 'ch8b',            str(result.get('ch8b')))
       parser.set('settings', 'ant9',            form.antennaport9.data)
       parser.set('settings', 'ch9f',            str(result.get('ch9f')))
       parser.set('settings', 'ch9b',            str(result.get('ch9b')))
       parser.set('settings', 'ant10',           form.antennaport10.data)
       parser.set('settings', 'ch10f',           str(result.get('ch10f')))
       parser.set('settings', 'ch10b',           str(result.get('ch10b')))
       parser.set('settings', 'ant11',           form.antennaport11.data)
       parser.set('settings', 'ch11f',           str(result.get('ch11f')))
       parser.set('settings', 'ch11b',           str(result.get('ch11b')))
       parser.set('settings', 'ant12',           form.antennaport12.data)
       parser.set('settings', 'ch12f',           str(result.get('ch12f')))
       parser.set('settings', 'ch12b',           str(result.get('ch12b')))
       parser.set('settings', 'ant13',           form.antennaport13.data)
       parser.set('settings', 'ch13f',           str(result.get('ch13f')))
       parser.set('settings', 'ch13b',           str(result.get('ch13b')))
       parser.set('settings', 'ant14',           form.antennaport14.data)
       parser.set('settings', 'ch14f',           str(result.get('ch14f')))
       parser.set('settings', 'ch14b',           str(result.get('ch14b')))
       parser.set('settings', 'ant15',           form.antennaport15.data)
       parser.set('settings', 'ch15f',           str(result.get('ch15f')))
       parser.set('settings', 'ch15b',           str(result.get('ch15b')))
     
       fp = open('config.ini','w')
       parser.write(fp)
       fp.close()

   ringbufferPath = parser['settings']['ringbuffer_path']
   form.antennaport0.data =     parser['settings']['ant0']
   form.antennaport1.data =     parser['settings']['ant1']
   form.antennaport2.data =     parser['settings']['ant2']
   form.antennaport3.data =     parser['settings']['ant3']
   form.antennaport4.data =     parser['settings']['ant4']
   form.antennaport5.data =     parser['settings']['ant5']
   form.antennaport6.data =     parser['settings']['ant6']
   form.antennaport7.data =     parser['settings']['ant7']
   form.antennaport8.data =     parser['settings']['ant8']
   form.antennaport9.data =     parser['settings']['ant9']
   form.antennaport10.data =     parser['settings']['ant10']
   form.antennaport11.data =     parser['settings']['ant11']
   form.antennaport12.data =     parser['settings']['ant12']
   form.antennaport13.data =     parser['settings']['ant13']
   form.antennaport14.data =     parser['settings']['ant14']
   form.antennaport15.data =     parser['settings']['ant15']
   ch0f =     parser['settings']['ch0f']
   ch0b =     parser['settings']['ch0b']     
   ch1f =     parser['settings']['ch1f']
   ch1b =     parser['settings']['ch1b']
   ch2f =     parser['settings']['ch2f']
   ch2b =     parser['settings']['ch2b']
   ch3f =     parser['settings']['ch3f']
   ch3b =     parser['settings']['ch3b']
   ch4f =     parser['settings']['ch4f']
   ch4b =     parser['settings']['ch4b']
   ch5f =     parser['settings']['ch5f']
   ch5b =     parser['settings']['ch5b']
   ch6f =     parser['settings']['ch6f']
   ch6b =     parser['settings']['ch6b']
   ch7f =     parser['settings']['ch7f']
   ch7b =     parser['settings']['ch7b']
   ch8f =     parser['settings']['ch8f']
   ch8b =     parser['settings']['ch8b']
   ch9f =     parser['settings']['ch9f']
   ch9b =     parser['settings']['ch9b']
   ch10f =     parser['settings']['ch10f']
   ch10b =     parser['settings']['ch10b']
   ch11f =     parser['settings']['ch11f']
   ch11b =     parser['settings']['ch11b']
   ch12f =     parser['settings']['ch12f']
   ch12b =     parser['settings']['ch12b']
   ch13f =     parser['settings']['ch13f']
   ch13b =     parser['settings']['ch13b']
   ch14f =     parser['settings']['ch14f']
   ch14b =     parser['settings']['ch14b']
   ch15f =     parser['settings']['ch15f']
   ch15b =     parser['settings']['ch15b']
   print("F: ringbufferPath=",ringbufferPath)
   return render_template('desetup.html',
	  ringbufferPath = ringbufferPath,
      form = form, status = theStatus,
      ch0f = ch0f, ch0b = ch0b,
	  ch1f = ch1f, ch1b = ch1b,
      ch2f = ch2f, ch2b = ch2b,
	  ch3f = ch3f, ch3b = ch3b,
	  ch4f = ch4f, ch4b = ch4b,
	  ch5f = ch5f, ch5b = ch5b,
	  ch6f = ch6f, ch6b = ch6b,
	  ch7f = ch7f, ch7b = ch7b,
	  ch8f = ch8f, ch8b = ch8b,
	  ch9f = ch9f, ch9b = ch9b,
	  ch10f = ch10f, ch10b = ch10b,
	  ch11f = ch11f, ch11b = ch11b,
	  ch12f = ch12f, ch12b = ch12b,
	  ch13f = ch13f, ch13b = ch13b,
	  ch14f = ch14f, ch14b = ch14b,
	  ch15f = ch15f, ch15b = ch15b )

# start propagation monitoring for FT8
def startprop():
  global thePropStatus
  theCommand = 'SF'
  print("start FT8 monitoring")
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("F: connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("F: send command")
     tcp_client.sendall(data.encode())
  except Exception as e: 
     print(e)
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error: mainctl program not responding; please restart it"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     thePropStatus = 1
  return 

# stop propagation monitoring for FT8
def stopprop():
  global thePropStatus
  theCommand = 'XF'
  print("stop FT8 monitoring")
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("F: connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("F: send command")
     tcp_client.sendall(data.encode())
  except Exception as e: 
     print(e)
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error: mainctl program not responding; please restart it"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     thePropStatus = 0
  return 


#@app.route("/startcollection")
def startcoll():
  form = MainControlForm()
  global theStatus, theDataStatus
  print("F: Start Data Collection command")
  parser = configparser.ConfigParser(allow_no_value=True)
  parser.read('config.ini')
  ringbufferPath = parser['settings']['ringbuffer_path']
  try:
    dlist = os.listdir(ringbufferPath)
  except Exception as e: 
     print(e)
     theDataStatus = "Path for saving data nonexistent or invalid: '" + ringbufferPath + "'"
     dataCollStatus = 0
     form.dataStat = theDataStatus
     return   
  theCommand = 'SC'
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("F: connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("F: send command")
     tcp_client.sendall(data.encode())
  except Exception as e: 
     print(e)
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error: mainctl program not responding; please restart it"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()
     theDataStatus = "Started data collection"
     dataCollStatus = 1
     form.dataStat = theDataStatus
  return 



#@app.route("/stopcollection")
def stopcoll():
  form = MainControlForm()
  global theStatus, theDataStatus
  print("F: Stop Data Collection command")
  theCommand = 'XC'
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("F: connect to socket")
     tcp_client.connect((host_ip, server_port))
     print("F: send command")
     tcp_client.sendall(data.encode())
  except Exception as e: 
     print(e)

     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error-mainctl program not responding, please restart it"
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
     form.throttle.data = parser['settings']['throttle']
     return render_template('throttle.html',
	  form = form)

   if request.method == 'POST':
     result = request.form
     print("F: result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("F: CANCEL")
     else:
       print("F: result of throttle post =")
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
     print("F: result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("F: CANCEL")
     else:
       print("F: result of callsign post =")
       ringbufferPath = ""

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
   form = ServerControlForm()
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   theStatus = ""
   if request.method == 'GET':
     print("F: smtpsvr = ", parser['email']['smtpsvr'])
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
      smtppw = smtppw, status = theStatus, form = form)


   form = ServerControlForm()
   if not form.validate():
     result = request.form
     emailto = result.get('emailto')
     smtpsvr = result.get('smtpsvr')
     emailfrom = result.get('emailfrom')
     smtpport = result.get('smtpport')
     smtptimeout = result.get('smtptimeout')
     smtppw = result.get('smtppw')
     smtpuid = result.get('smtpuid')
     print("email to="+emailto)
     print("smtpsvr="+smtpsvr)
     theStatus = form.errors

     result = request.form   


     return render_template('notification.html',
	  smtpsvr = smtpsvr, emailfrom = emailfrom,
     emailto = emailto, smtpport = smtpport,
      smtptimeout = smtptimeout, smtpuid = smtpuid,
      smtppw = smtppw, status = theStatus, form=form)


   if request.method == 'POST':
     result = request.form
     print("F: result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("F: CANCEL")
     elif result.get('csubmit') == "Send test email" :
        print("Send test email")
        try:
          msg = MIMEMultipart()
          smtpsvr =     parser['email']['smtpsvr']
          msg['From'] = parser['email']['emailfrom']
          msg['To'] =   parser['email']['emailto']
          msg['Subject'] = "Test message from your TangerineSDR"
          smtpport =    parser['email']['smtpport']
          smtptimeout = parser['email']['smtptimeout']
          smtpuid =     parser['email']['smtpuid']
          smtppw =      parser['email']['smtppw']
          body = "Test message from your TangerineSDR"
          msg.attach(MIMEText(body,'plain'))
          server = smtplib.SMTP(smtpsvr,smtpport,'None',int(smtptimeout))
          server.ehlo()
          server.starttls()
          server.login(smtpuid,smtppw)
          text = msg.as_string()
          server.sendmail(parser['email']['emailfrom'],parser['email']['emailto'],text)
          print("sendmail done")
          theStatus = "Test mail sent."
        except Exception as e: 
         print(e)
         theStatus = e
        
     else:
        print("F: reached POST on notification;", result.get('smtpsvr'))
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
      smtppw = smtppw, status = theStatus)

@app.route("/propagation",methods=['POST','GET'])
def propagation():
   global theStatus, theDataStatus
   form = ChannelControlForm()
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')

   if request.method == 'GET':
     form.antennaport0.data =     parser['settings']['ftant0']
     form.antennaport1.data =     parser['settings']['ftant1']
     form.antennaport2.data =     parser['settings']['ftant2']
     form.antennaport3.data =     parser['settings']['ftant3']
     form.antennaport4.data =     parser['settings']['ftant4']
     form.antennaport5.data =     parser['settings']['ftant5']
     form.antennaport6.data =     parser['settings']['ftant6']
     form.antennaport7.data =     parser['settings']['ftant7']
     ft80f =     parser['settings']['ft80f'] 
     ft81f =     parser['settings']['ft81f'] 
     ft82f =     parser['settings']['ft82f']
     ft83f =     parser['settings']['ft83f']
     ft84f =     parser['settings']['ft84f']
     ft85f =     parser['settings']['ft85f']
     ft86f =     parser['settings']['ft86f']
     ft87f =     parser['settings']['ft87f']
     return render_template('ft8setup.html',
      form  = form,
      ft80f = ft80f,
	  ft81f = ft81f,
      ft82f = ft82f,
	  ft83f = ft83f, 
	  ft84f = ft84f, 
	  ft85f = ft85f,
	  ft86f = ft86f, 
	  ft87f = ft87f  )

   if request.method == 'POST':
     result = request.form
     print("F: result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("F: CANCEL")
     else:
       print("F: POST ringbufferPath =", result.get('ringbufferPath'))
       ringbufferPath = ""
       parser.set('settings', 'ringbuffer_path', result.get('ringbufferPath'))
       parser.set('settings', 'ftant0',            form.antennaport0.data)
       parser.set('settings', 'ft80f',            str(result.get('ft80f')))
       parser.set('settings', 'ftant1',            form.antennaport1.data)
       parser.set('settings', 'ft81f',            str(result.get('ft81f')))
       parser.set('settings', 'ftant2',            form.antennaport2.data)
       parser.set('settings', 'ft82f',            str(result.get('ft82f')))
       parser.set('settings', 'ftant3',            form.antennaport3.data)
       parser.set('settings', 'ft83f',            str(result.get('ft83f')))
       parser.set('settings', 'ftant4',            form.antennaport4.data)
       parser.set('settings', 'ft84f',            str(result.get('ft84f')))
       parser.set('settings', 'ftant5',            form.antennaport5.data)
       parser.set('settings', 'ft85f',            str(result.get('ft85f')))
       parser.set('settings', 'ftant6',            form.antennaport6.data)
       parser.set('settings', 'ft86f',            str(result.get('ft86f')))
       parser.set('settings', 'ftant7',            form.antennaport7.data)
       parser.set('settings', 'ft87f',            str(result.get('ft87f')))  
       fp = open('config.ini','w')
       parser.write(fp)
       fp.close()
     ringbufferPath = parser['settings']['ringbuffer_path']
     form.antennaport0.data =     parser['settings']['ftant0']
     form.antennaport1.data =     parser['settings']['ftant1']
     form.antennaport2.data =     parser['settings']['ftant2']
     form.antennaport3.data =     parser['settings']['ftant3']
     form.antennaport4.data =     parser['settings']['ftant4']
     form.antennaport5.data =     parser['settings']['ftant5']
     form.antennaport6.data =     parser['settings']['ftant6']
     form.antennaport7.data =     parser['settings']['ftant7']
     ft80f =     parser['settings']['ft80f']
     ft81f =     parser['settings']['ft81f']
     ft82f =     parser['settings']['ft82f']
     ft83f =     parser['settings']['ft83f']
     ft84f =     parser['settings']['ft84f']
     ft85f =     parser['settings']['ft85f']
     ft86f =     parser['settings']['ft86f']
     ft87f =     parser['settings']['ft87f']
     return render_template('ft8setup.html',
      form = form,
      ft80f = ft80f,
	  ft81f = ft81f,
      ft82f = ft82f, 
	  ft83f = ft83f,
	  ft84f = ft84f, 
	  ft85f = ft85f, 
	  ft86f = ft86f, 
	  ft87f = ft87f  )


######################################################################
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

