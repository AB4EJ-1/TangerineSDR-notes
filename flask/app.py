from flask import Flask, flash, redirect, render_template, request, session, abort, jsonify, Response
import simplejson as json
import socket
import _thread
import time
import os
import subprocess
from subprocess import Popen, PIPE
import configparser
import smtplib

# This is temporary until we can get digital_rf, h5py, and hdf5 to install correctly under Python3.x
import sys

import h5py
import numpy as np
import datetime

#from array import *
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
from forms import CallsignForm

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
received = ""
dataCollStatus = 0;
theStatus = "Not yet started"
theDataStatus = ""
thePropStatus = 0
f = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]

# Here are commands that can be sent to mainctl and forwarded to DE.
# These must be the same as the corresponding mnemonics in the de_signals.h file
# used for mainctl compilation (also DESimulator, if simulator is being used)
STATUS_INQUIRY     = "S?"
LED1_ON            = "Y1"
LED1_OFF           = "N1"
TIME_INQUIRY       = "T?"
TIME_STAMP         = "TS"
CREATE_CHANNEL     = "CC"
CONFIG_CHANNELS    = "CH"
UNDEFINE_CHANNEL   = "UC"
FIREHOSE_SERVER    = "FH"
START_DATA_COLL    = "SC"
STOP_DATA_COLL     = "XC"
DEFINE_FT8_CHAN    = "FT"
START_FT8_COLL     = "SF"
STOP_FT8_COLL      = "XF"
LED_SET            = "SB"  
UNLINK             = "UL"
HALT_DE            = "XX"

def send_to_mainctl(cmdToSend,waitTime):
  global theStatus
  print("F: sending:" + cmdToSend)
  host_ip, server_port = "127.0.0.1", 6100
  data = cmdToSend + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("F: *** WC: *** connect to socket, port ", server_port)
     tcp_client.connect((host_ip, server_port))
     print("F: send cmd:",cmdToSend)
     tcp_client.sendall(data.encode())
     print("F: wait for DE response")
     if(waitTime > 0):
       time.sleep(waitTime)
     print("F: try to receive response")
     received = "NO RESPONSE"
    # Read data from the TCP server and close the connection
     try:
       received = tcp_client.recv(1024, socket.MSG_DONTWAIT)
       print("F: received data from DE: ", received)

     except Exception as e:
       print("F: exception on recv")
       theStatus = "Mainctl stopped or DE disconnected , error: " + str(e)
     theStatus = "Active"
     print("F: mainctl answered ", received, " thestatus = ",theStatus)

#     print("F: LH answered ", received, " substr = '", received[0:2].decode("ASCII"), "'")
#     if(received[0:3].decode("ASCII") == "ACK"):
#       print("F: received ACK")
#       theStatus = "ON"
  except Exception as e: 
     print(e)
     print("F: '" + e.errno + "'")
     if(str(e.errno) == "111" or str(e.errno == "11")):
       theStatus = "Error " + e.errno +  "mainctl program not responding"
     else:
       theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()


def channel_request():
  print("Send channel creation request")   
  parser = configparser.ConfigParser(allow_no_value=True)
  parser.read('config.ini')
# ports that mainctl will listen on for traffic from DE
  configPort =  parser['settings']['configport']
  dataPort   =  parser['settings']['dataport']
# commas must separate fields for token processing to work in mainctl
  send_to_mainctl((CREATE_CHANNEL + "," + configPort + "," + dataPort),1 )
  
def check_status_once():
  global theStatus
  send_to_mainctl(STATUS_INQUIRY,4)
  print("after check status once, theStatus=",theStatus)
  return

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
         print('F: mode set to:"',form.mode.data,'"')
         parser.set('settings','mode',form.mode.data)
         fp = open('config.ini','w')
         parser.write(fp)
         fp.close()
         print('F: start set to ',form.startDC.data)
         print('F: stop set to ', form.stopDC.data)
         if(form.startDC.data and form.mode.data =='snapshotter'):
           process = subprocess.Popen(["./displayFFT.py","/mnt/RAM_disk/snap/fn.dat"], stdout = PIPE, stderr=PIPE)
#           stdout, stderr = process.communicate()
#           print(stdout)
           return  render_template('tangerine.html', form = form)
         if(form.startDC.data ):
            if ( len(parser['settings']['ringbuffer_path']) < 1 
                   and form.mode.data == 'ringbuffer') :
              print("F: configured ringbuffer path='", parser['settings']['ringbuffer_path'],"'", len(parser['settings']['ringbuffer_path']))
              form.errline = 'ERROR: Path to digital data storage not configured'
            else:
          #    startcoll()
          # command mainctl to trigger DE to start sending ringvuffer data
              send_to_mainctl(START_DATA_COLL,1)
# write metadata describing channels into the drf_properties file
              ant = []
              chf = []
              bw = []
              chcount = 0
              for i in range(0,15):
                if(parser['settings']['ant' + str(i)] != 'Off'):
                  ant.append(int(parser['settings']['ant' + str(i)]))
                  chf.append(float(parser['settings']['ch' + str(i) + 'f']))
                  bw.append(float(parser['settings']['ch' + str(i) + 'b']))
                  chcount = chcount + 1
              print("Record list of subchannels=",chf)
              f5 = h5py.File('/media/odroid/416BFA3A615ACF0E/hamsci/hdf5/drf_properties.h5','r+')
              f5.attrs.__setitem__('no_of_subchannels',chcount)
              f5.attrs.__setitem__('subchannel_frequencies_MHz', chf)
              f5.attrs.__setitem__('subchannel_bandwidths_KHz',bw)
              f5.attrs.__setitem__('antenna_ports',ant)
              f5.close()
         if(form.stopDC.data ):
 #           stopcoll()
            send_to_mainctl(STOP_DATA_COLL,1)
         if(form.startprop.data):
            startprop()
         if(form.stopprop.data) :
            stopprop()
         print("F: end of control loop; theStatus=", theStatus)
         form.destatus = theStatus
         form.dataStat = theDataStatus
         return render_template('tangerine.html', form = form)


@app.route("/restart3")
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
   check_status_once()
   print("RESTART: status = ",theStatus, " received = ", received)
   return redirect('/')


@app.route("/chkstat")
def chkstat():
   global theStatus, theDatastatus
#   print("Checking status...")
#   theStatus = check_status_once();
   print("Sending channel req")
   channel_request()
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

   result = request.form
   rgPathExists = os.path.isdir(result.get('ringbufferPath'))
   print("path / directory existence check: ", rgPathExists)
   
   if not form.validate() or rgPathExists == False:
     if rgPathExists == True:
       theStatus = form.errors
     else:
       theStatus = "Ringbuffer path invalid or not a directory"
#     result = request.form
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
#       ringbufferPath = ""
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
   configport =parser['settings']['configport']
   dataport =  parser['settings']['dataport']
   print("F: ringbufferPath=",ringbufferPath)

# build & send combined channel definition to mainctl

   a = [form.antennaport0.data, form.antennaport1.data, 
        form.antennaport2.data,  form.antennaport3.data,
        form.antennaport4.data,  form.antennaport5.data,
        form.antennaport6.data,  form.antennaport7.data,
        form.antennaport8.data,  form.antennaport9.data,
        form.antennaport10.data, form.antennaport11.data,
        form.antennaport12.data, form.antennaport13.data,
        form.antennaport14.data, form.antennaport15.data ]

   f = [ ch0f, ch1f, ch2f, ch3f, ch4f,
                    ch5f, ch6f, ch7f, ch8f, ch9f,
                    ch10f, ch11f, ch12f, ch13f, ch14f, ch15f]

   b = [ch0b, ch1b, ch2b, ch3b, ch4b, ch5b,
                   ch6b, ch7b, ch8b, ch9b, ch10b, ch11b,
                   ch12b, ch13b, ch14b, ch15b ]

   configCmd = CONFIG_CHANNELS
   for i in list(range(16)):
     configCmd = configCmd + ',' + str(i) + ',' + a[i] + ',' + f[i] + ',' + b[i]

#   print("configcmd=" + configCmd)
   send_to_mainctl(configCmd,1);

# record a DigitalMetatdata file including channel config details
#   metadata_dir = ringbufferPath
#   subdirectory_cadence_seconds = 3600
#   file_cadence_seconds = 60
#   samples_per_second_numerator = 10
#   samples_per_second_denominator = 9
#   file_name = "channel_layout"
#   stime =int(time.time())
#   dmw = digital_rf.DigitalMetadataWriter(
#    metadata_dir,
#    subdirectory_cadence_seconds,
#    file_cadence_seconds,
#    samples_per_second_numerator,
#    samples_per_second_denominator,
#    file_name,
#    )
   print("first metatdata create okay")

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
  parser = configparser.ConfigParser(allow_no_value=True)
  parser.read('config.ini')
  ft80f  =     parser['settings']['ft80f'] 
  ftant0 =     parser['settings']['ftant0']
  if (ftant0 == 'Off'): 
    ftant0 = '-1'
  ft81f  =     parser['settings']['ft81f'] 
  ftant1 =     parser['settings']['ftant1']
  if (ftant1 == 'Off'): 
    ftant1 = '-1'
  ft82f  =     parser['settings']['ft82f']
  ftant2 =     parser['settings']['ftant2']
  if (ftant2 == 'Off'): 
    ftant2 = '-1'
  ft83f  =     parser['settings']['ft83f']
  ftant3 =     parser['settings']['ftant3']  
  if (ftant3 == 'Off'): 
    ftant3 = '-1'
  ft84f  =     parser['settings']['ft84f']
  ftant4 =     parser['settings']['ftant4']
  if (ftant4 == 'Off'): 
    ftant4 = '-1'
  ft85f  =     parser['settings']['ft85f']
  ftant5 =     parser['settings']['ftant5']  
  if (ftant5 == 'Off'): 
    ftant5 = '-1'
  ft86f  =     parser['settings']['ft86f']
  ftant6 =     parser['settings']['ftant6']
  if (ftant6 == 'Off'): 
    ftant6 = '-1'
  ft87f  =     parser['settings']['ft87f']
  ftant7 =     parser['settings']['ftant7']
  if (ftant7 == 'Off'): 
    ftant7 = '-1'

  theCommand = START_FT8_COLL + ' ' + ftant0 + ' ' + ftant1 + ' ' + ftant2 + ' ' + \
                            ftant3 + ' ' + ftant4 + ' ' + ftant5 + ' ' + \
                            ftant6 + ' ' + ftant7 + ' ' + \
                            ft80f  + ' ' + ft81f  + ' ' + ft82f + ' ' + \
                            ft83f  + ' ' + ft84f  + ' ' + ft85f + ' ' + \
                            ft86f  + ' ' + ft87f
  print("start FT8 monitoring " + theCommand)
  host_ip, server_port = "127.0.0.1", 6100
  data = theCommand + "\n"  
  send_to_mainctl(theCommand,0)
  thePropStatus = 1
  return

# the below taken out of service
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Establish connection to TCP server and exchange data
     print("F: connect to socket, port ", server_port)
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
  send_to_mainctl(STOP_FT8_COLL,0)
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
  send_to_mainctl(START_DATA_COLL,0)
  theDataStatus = "Started data collection"
  dataCollStatus = 1
  form.dataStat = theDataStatus
  return


#@app.route("/stopcollection")
def stopcoll():
  form = MainControlForm()
  global theStatus, theDataStatus
  send_to_mainctl(STOP_DATA_COLL,1)
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
   form = CallsignForm()
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   if request.method == 'GET':
     c0 = parser['monitor']['c0']
     c1 = parser['monitor']['c1']
     c2 = parser['monitor']['c2']
     c3 = parser['monitor']['c3']
     c4 = parser['monitor']['c4']
     c5 = parser['monitor']['c5']
     return render_template('callsign.html', form = form,
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
     
     return render_template('callsign.html', form = form,
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
   psk = False
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
      pskindicator = psk,
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

@app.route('/_ft8list')
def ft8list():
  ft8string = ""
  band = [ 7, 10, 14, 18, 21, 24, 28]
  try:
#   f = open("/mnt/RAM_disk/FT8/decoded0.txt","r")
#   x = f.readlines()
#   f.close()
    plist = []
    for fno in range(7):
     fname = '/mnt/RAM_disk/FT8/decoded' + str(fno) +'.txt'
 #    print("checking file",fname)
     f = open(fname,"r")
     plist.append(len(f.readlines()))
     f.close()
      
# here we build a JSON string to populate the FT8 panel
    ft8string = '{'
#   for i in range(0,len(x)):
#    ft8string = ft8string + '"' + str(i) + '":"' +  \
#      x[i][39:46] + ' ' + x[i][53:57] + ' ' + x[i][30:32] + ' MHz",'
    ft8string = ft8string + '"0":"MHz  spots",'
    for i in range(7):
     pval = str(plist[i])
     ft8string = ft8string + '"' + str(i+1) + '":"' +  \
       str(band[i]) + ' - ' + pval + ' ",'

    ft8string = ft8string + '"end":" "}'
#    print("string= " , ft8string)
  except Exception as ex:
 #   print(ex)
# no-op
    z=1

  return Response(ft8string, mimetype='application/json')

######################################################################
@app.errorhandler(404)
def page_not_found(e):
    return render_template("notfound.html")


if __name__ == "__main__":
#	app.run(host='0.0.0.0')
#	app.run(debug = True)

	from waitress import serve
	serve(app, host = "0.0.0.0", port=5000) 
#	serve(app)
#	serve(app, host = "192.168.1.75", port=5000)

