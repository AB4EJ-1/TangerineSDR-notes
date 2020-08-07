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
from datetime import datetime
import threading

#from array import *
from email.mime.multipart import MIMEMultipart 
from email.mime.text import MIMEText 
from extensions import csrf
from config import Config
from flask_wtf import Form
# following is for future flask upgrade
#from FlaskForm import Form
from wtforms import TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField, DecimalField, FloatField
from flask import request, flash
from forms import MainControlForm, ThrottleControlForm, ChannelControlForm, ServerControlForm
from forms import CallsignForm
from forms import ChannelControlForm, ChannelForm, ChannelListForm

from wtforms import validators, ValidationError

from flask_wtf import CSRFProtect

app = Flask(__name__)
app.config['SECRET_KEY'] = 'here-is-your-secret-key-ghost-rider'
app.secret_key = 'development key'
app.config.from_object(Config)
csrf.init_app(app)

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
DATARATE_INQUIRY   = "R?"
DATARATE_RESPONSE  = "DR"
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

class hbThread (threading.Thread):
   def __init__(self, threadID, name, counter):
      threading.Thread.__init__(self)
      self.threadID = threadID
      self.name = name
      self.counter = counter
   def run(self):
      print ("Starting " + self.name)
      ping_mainctl()
      print ("Exiting " + self.name)

def is_numeric(s):
  try:
   float(s)
   return True
  except ValueError:
   print("Value error")
   return False

def send_to_mainctl(cmdToSend,waitTime):
  global theStatus, rateList
  print("F: sending:" + cmdToSend)
  host_ip, server_port = "127.0.0.1", 6100
  data = cmdToSend + "\n"  
    # Initialize a TCP client socket using SOCK_STREAM 
  try:
     print("F: define socket")
     tcp_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#    Settings to keep TCP port from disconnecting when not used for a while
     tcp_client.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE,1)
     tcp_client.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 1)
     tcp_client.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 3)
     tcp_client.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 15)
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
 #      print("F: received data from DE: ", received)
       d = received.decode()
     #  print("F: decoded:",d)
       print("F: buftype is '",received[0:2],"'")
       print("F: bytes=",received[0],"/",received[1],"/",received[2])
#       print("find:",received[0:2].find("DR"))
       if(d.find("DR") !=  -1):
         print("F: DR buffer received")
         parser = configparser.ConfigParser(allow_no_value=True)
         parser.read('config.ini')
         rateList = []
         a = d.split(":")
         b = a[1].split(";")
       
         print("b=",b)
         rateCount = 0
         for ratepair in b:
           c = ratepair.split(',')
           lenc = len(c[0])
           print("c[0]=",c[0]," len c[0]=",lenc)

           if(lenc > 3):
             break
           rateList.append(c)
           parser.set('datarates', 'r'+str(rateCount), c[1] )
           rateCount = rateCount + 1
           print("ratepair=",ratepair," c=",c, "c[1]=",c[1])
         print("rateList = ",rateList)
         parser.set('datarates','numrates',str(rateCount))
         fp = open('config.ini','w')
         parser.write(fp)
         fp.close()
     except Exception as e:
       print("F: exception on recv,")    # , e.message)
       theStatus = "Mainctl stopped or DE disconnected , error: " + str(e)
     theStatus = "Active"
     print("F: mainctl answered ", received, " thestatus = ",theStatus)

  except Exception as e: 
     print(e)
     theStatus = "Exception " + str(e)
  finally:
     tcp_client.close()

def channel_request():
  print("  * * * * * Send channel creation request * * * *")   
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

def ping_mainctl():
  while(1):
    time.sleep(60)
    localtime = time.asctime(time.localtime(time.time()))
    print("F: PING mainctl with S? at ", localtime)
    send_to_mainctl("S?",1)
    print("F: mainctl replied") 

def send_channel_config():  # send channel configuration command to DE
  global theStatus
  parser = configparser.ConfigParser(allow_no_value=True)
  parser.read('config.ini')
  channelcount = parser['channels']['numChannels']
  rate_list = []
  numRates = parser['datarates']['numRates']
  configCmd = CONFIG_CHANNELS + "," + channelcount + "," + parser['channels']['datarate'] + ","
  for ch in range(int(channelcount)):
    print("add channel ",ch)
    configCmd = configCmd + str(ch) + "," + parser['channels']['p' + str(ch)] + ","
    configCmd = configCmd + parser['channels']['f' + str(ch)] + ","
  print("Sending CH config command to DE")

  send_to_mainctl(configCmd,1);
  return


#####################################################################
# Here is the home page
@app.route("/", methods = ['GET', 'POST'])
def sdr():
   form = MainControlForm()
   global theStatus, theDataStatus
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
  # print("CSRF time limit=" + WTF_CSRF_TIME_LIMIT + " ;")
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
         parser.set('settings','mode',form.mode.data)  # update config file to reflect mode setting
         fp = open('config.ini','w')
         parser.write(fp)
         fp.close()
         print('F: start set to ',form.startDC.data)
         print('F: stop set to ', form.stopDC.data)

# following code is a demo for how to make GNURadio show FFT of a file.
# file name is temporarily hard-coded in displayFFT.py routine
 #        if(form.startDC.data and form.mode.data =='snapshotter'):
  #         process = subprocess.Popen(["./displayFFT.py","/mnt/RAM_disk/snap/fn.dat"], stdout = PIPE, stderr=PIPE)
#           stdout, stderr = process.communicate()
#           print(stdout)
#        return  render_template('tangerine.html', form = form)

         if(form.startDC.data ):
            if   ( len(parser['settings']['firehoser_path']) < 1 
                   and form.mode.data == 'firehoseR') :
              print("F: configured temp firehose path='", parser['settings']['firehoser_path'],"'", len(parser['settings']['firehoser_path']))
              form.errline = 'ERROR: Path to temporary firehoseR storage not configured'
            elif ( len(parser['settings']['ringbuffer_path']) < 1 
                   and form.mode.data == 'ringbuffer') :
              print("F: configured ringbuffer path='", parser['settings']['ringbuffer_path'],"'", len(parser['settings']['ringbuffer_path']))
              form.errline = 'ERROR: Path to digital data storage not configured'
            else:

          # User wants to start data collection. Is there an existing drf_properties file?

              now = datetime.now()
         #     subdir = "D" + now.strftime('%Y%m%d%H%M%S')
              subdir = "TangerineData"
              print("SEND START DATA COLLECTION COMMAND, subdirectory=" + subdir)
              if(form.mode.data == 'firehoseR'):
                metadataPath = parser['settings']['firehoser_path'] + "/" + subdir
              else:
                metadataPath = parser['settings']['ringbuffer_path'] + "/" + subdir
           #   print("metadata path="+metadataPath)
              returned_value = os.system("mkdir "+ metadataPath)
              print("F: after metadata creation, retcode=",returned_value)
          # command mainctl to trigger DE to start sending ringbuffer data
              send_to_mainctl(START_DATA_COLL + "," + subdir,1)
              dataCollStatus = 1
# write metadata describing channels into the drf_properties file
              ant = []
              chf = []
              chcount = int(parser['channels']['numchannels'])
              datarate = int(parser['channels']['datarate'])
              for i in range(0,chcount):
                ant.append(int(parser['channels']['p' + str(i)]))
                chf.append(float(parser['channels']['f' + str(i)]))
              print("Record list of subchannels=",chf)
              
              try:
                print("Update properties file")
           #     print("Removed temporarily for debugging")
                f5 = h5py.File(metadataPath + '/drf_properties.h5','r+')
                f5.attrs.__setitem__('no_of_subchannels',chcount)
                f5.attrs.__setitem__('subchannel_frequencies_MHz', chf)
              #  f5.attrs.__setitem__('data_rate',datarate)
                f5.attrs.__setitem__('antenna_ports',ant)
                f5.close()
              except:
                print("WARNING: unable to update DRF HDF5 properties file")

         if(form.stopDC.data ):
            send_to_mainctl(STOP_DATA_COLL,1)
            dataCollStatus = 0;
         if(form.startprop.data):
            startprop()
         if(form.stopprop.data) :
            stopprop()
         print("F: end of control loop; theStatus=", theStatus)
         form.destatus = theStatus
         form.dataStat = theDataStatus
         return render_template('tangerine.html', form = form)

@app.route("/restart") # restarts mainctl program
def restart():
   global theStatus, theDataStatus
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   print("F: restart")
  # send_to_mainctl("XX",1)
   returned_value = os.system("killall -9 mainctl")
   print("F: after killing mainctl, retcode=",returned_value)
   print("F: Trying to restart mainctl")
# start mainctl as independent process
#   returned_value = os.system("/home/odroid/projects/TangerineSDR-notes/mainctl/mainctl")
 # start mainctl as a subprocess
   returned_value = subprocess.Popen("/home/odroid/projects/TangerineSDR-notes/mainctl/mainctl")
  # returned_value = os.system("/home/odroid/projects/TangerineSDR-notes/mainctl/mainctl 1>> main.log &")
   time.sleep(3)
   print("F: after restarting mainctl, retcode=",returned_value)
#   stopcoll()
#   check_status_once()
   print("RESTART: status = ",theStatus, " received = ", received)
   print("Call Channel Request")
   channel_request()
   print("Request Data Rate List")
   send_to_mainctl(DATARATE_INQUIRY,0.1)

   send_channel_config()
   print("F: Config Channel sent");

# ringbuffer setup
   ringbufferPath =    parser['settings']['ringbuffer_path']
   ringbufferMaxSize = parser['settings']['ringbuffer_max_size']
# halt any previously started ringbuffer task(s)
   rcmd = 'killall -9 drf'
   returned_value = os.system(rcmd)
   rcmd = 'drf ringbuffer -z ' + ringbufferMaxSize + ' -p 120 -v ' + ringbufferPath + ' &'
# spin off this process asynchornously (notice the & at the end)
   returned_value = os.system(rcmd)
   print("F: ringbuffer control activated")
# start heartbeat thread that pings mainctl at intervals
   thread1 = hbThread(1, "pingThread-1",1)
   thread1.start()
   return redirect('/')

@app.route("/datarates")
def datarates():
  global theStatus
  print("Request datarates")
  send_to_mainctl(DATARATE_INQUIRY,0.1)
#  print("after check status once, theStatus=",theStatus)
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

@app.route("/desetup1",methods=['POST','GET'])
def desetup1():
   print("hit desetup1; request.method=",request.method)
   global theStatus, theDataStatus
   form = ChannelControlForm()
   channellistform = ChannelListForm()

#   form.chp_setting = [('0'),('0')]
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   ringbufferPath = parser['settings']['ringbuffer_path']
   theStatus = ""
   if request.method == 'GET':
# temporary.   This list must be built based on DE report of available data rates
    rate =[('4000',4000),('8000',8000),('12000',12000),('24000',24000)]
    rate_list = []
    for r in range(3):
      rate_list.append(rate[r])
 #   print("rate_list=",rate_list)
    form.channelrate.choices = rate_list
    
    print("channellistform channels=",channellistform.channels)
    return render_template('desetup1.html',
	  ringbufferPath = ringbufferPath,
      form = form, status = theStatus,
      channellistform = channellistform)

   if request.method == 'POST':
      result = request.form
      ringbufferPath = parser['settings']['ringbuffer_path']
      print("F: result=", result.get('csubmit'))
      if result.get('csubmit') == "Set no. of channels":
        channelcount = result.get('channelcount')
#        channellistform.channels.min_entries = channelcount
        print("set #channels to ",channelcount)
        form.port_list = []
        form.freq_list = []
        form.rate_list = []       
        for ch in range(int(channelcount)):
          channelform = ChannelForm()
          channelform.channel_freq = 0.0
          channellistform.channels.append_entry(channelform)

        print("return to desetup2")
        return render_template('desetup2.html',
	      ringbufferPath = ringbufferPath, channelcount = channelcount,
          form = form, status = theStatus,
          channellistform = channellistform)

@app.route("/desetup",methods=['POST','GET'])
def desetup():
   global theStatus, theDataStatus
   print("hit desetup2; request.method=",request.method)
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   ringbufferPath = parser['settings']['ringbuffer_path']
 #  form = ChannelControlForm()
   if request.method == 'GET' :
    channellistform = ChannelListForm()
# populate channel settings from config file
    channelcount = parser['channels']['numChannels']
    form = ChannelControlForm()
    form.channelcount.data = channelcount
    rate_list = []
# populate rate capabilities from config file.
# The config file should have been updated from DE sample rate list buffer.
    numRates = parser['datarates']['numRates']
    for r in range(int(numRates)):
      theRate = parser['datarates']['r'+str(r)]
      theTuple = [ str(theRate), int(theRate) ]
      rate_list.append(theTuple)

    form.channelrate.choices = rate_list
    rate1 = int(parser['channels']['datarate'])
 #   print("rate1 type = ",type(rate1))
    form.channelrate.data = rate1

    for ch in range(int(channelcount)):
      channelform = ChannelForm()
      channelform.channel_ant  = parser['channels']['p' + str(ch)] 
      channelform.channel_freq = parser['channels']['f' + str(ch)]
      channellistform.channels.append_entry(channelform)
    return render_template('desetup.html',
	  ringbufferPath = ringbufferPath, channelcount = channelcount,
      channellistform = channellistform,
      form = form, status = theStatus)

# if we arrive here, user has hit one of the buttons on page

   result = request.form

   print("F: result=", result.get('csubmit'))

# does user want to start over?
   if result.get('csubmit') == "Discard Changes" :
    channellistform = ChannelListForm()
# populate channel settings from config file
    channelcount = parser['channels']['numChannels']
    form = ChannelControlForm()
    form.channelcount.data = channelcount
    rate_list = []
# populate rate capabilities from config file.
# The config file should have been updated from DE sample rate list buffer.
    numRates = parser['datarates']['numRates']
    for r in range(int(numRates)):
      theRate = parser['datarates']['r'+str(r)]
      theTuple = [ str(theRate), int(theRate) ]
      rate_list.append(theTuple)

    form.channelrate.choices = rate_list
    rate1 = int(parser['channels']['datarate'])
 #   print("rate1 type = ",type(rate1))
    form.channelrate.data = rate1

    for ch in range(int(channelcount)):
      channelform = ChannelForm()
      channelform.channel_ant  = parser['channels']['p' + str(ch)] 
      channelform.channel_freq = parser['channels']['f' + str(ch)]
      channellistform.channels.append_entry(channelform)
    return render_template('desetup.html',
	  ringbufferPath = ringbufferPath, channelcount = channelcount,
      channellistform = channellistform,
      form = form, status = theStatus)


# did user hit the Set channel count button?

   if result.get('csubmit') == "Set no. of channels":
     channelcount = result.get('channelcount')
     print("set #channels to ",channelcount)
     channellistform = ChannelListForm()
     form = ChannelControlForm()
     form.channelcount.data = channelcount
     rate_list = []
     numRates = parser['datarates']['numRates']
     for r in range(int(numRates)):
      theRate = parser['datarates']['r'+str(r)]
      theTuple = [ str(theRate), int(theRate) ]
      rate_list.append(theTuple)
     form.channelrate.choices = rate_list
     rate1 = int(parser['channels']['datarate'])
     form.channelrate.data = rate1
     for ch in range(int(channelcount)):
  #    print("add channel ",ch)
      channelform = ChannelForm()
      channelform.channel_ant  = parser['channels']['p' + str(ch)] 
      channelform.channel_freq = parser['channels']['f' + str(ch)]
      channellistform.channels.append_entry(channelform)
     print("return to desetup")
     return render_template('desetup.html',
	      ringbufferPath = ringbufferPath, channelcount = channelcount,
          form = form, status = theStatus,
          channellistform = channellistform)

# user wants to save changes; update configuration file

# The range validation is done in code here due to problems with the
# WTForms range validator inside a FieldList

   if result.get('csubmit') == "Save Changes":
     statusCheck = True
     theStatus = "ERROR-"
     channelcount = result.get('channelcount')
     channelrate = result.get('channelrate')
     print("set data rate to ", channelrate)
     parser.set('channels','datarate',channelrate)
   #  theStatus = ""
     print("set #channels to ",channelcount)
     parser.set('channels','numChannels',channelcount)
     print("RESULT: ", result) 

     rgPathExists = os.path.isdir(result.get('ringbufferPath'))
     print("path / directory existence check: ", rgPathExists)
   
     if rgPathExists == False:
      theStatus = "Ringbuffer path invalid or not a directory. "
      statusCheck = False
     elif dataCollStatus == 1:
      theStatus = theStatus + "ERROR: you must stop data collection before saving changes here. "
      statusCheck = False

# save channel config to config file
     for ch in range(int(channelcount)):
       p = 'channels-' + str(ch) + '-channel_ant'
       parser.set('channels','p' + str(ch), result.get(p))
       print("p = ",p)
       print("channel #",ch," ant:",result.get(p))
       f = 'channels-' + str(ch) + '-channel_freq'
       fstr = result.get(f)
       if(is_numeric(fstr)):
         fval = float(fstr)
         if(fval < 0.1 or fval > 54.0):
           theStatus = theStatus +  "Freq for channel "+ str(ch) + " out of range;"
           statusCheck = False
         else:
          parser.set('channels','f' + str(ch), result.get(f))
       else:
         theStatus = theStatus + "Freq for channel " + str(ch) + " must be numeric;"
         statusCheck = False

    
     if(statusCheck == True):
       print("Save config; ringbuffer_path=" + result.get('ringbufferPath'))
       parser.set('settings', 'ringbuffer_path', result.get('ringbufferPath'))
       fp = open('config.ini','w')
       parser.write(fp)
       fp.close()
   



     channellistform = ChannelListForm()
     channelcount = parser['channels']['numChannels']
     form = ChannelControlForm()
     form.channelcount.data = channelcount
     rate_list = []
     numRates = parser['datarates']['numRates']
     for r in range(int(numRates)):
      theRate = parser['datarates']['r'+str(r)]
      theTuple = [ str(theRate), int(theRate) ]
      rate_list.append(theTuple)
     form.channelrate.choices = rate_list
     print("set channelrate to ", parser['channels']['datarate'])
     rate1 = int( parser['channels']['datarate'])
     form.channelrate.data = rate1
     rate_list = []
     numRates = parser['datarates']['numRates']
     for r in range(int(numRates)):
      theRate = parser['datarates']['r'+str(r)]
      theTuple = [ str(theRate), int(theRate) ]
      rate_list.append(theTuple)
     form.channelrate.choices = rate_list

 #    configCmd = CONFIG_CHANNELS + "," + channelcount + "," + parser['channels']['datarate'] + ","

     for ch in range(int(channelcount)):
  #    print("add channel ",ch)
      channelform = ChannelForm()
      channelform.channel_ant  = parser['channels']['p' + str(ch)] 
  #    configCmd = configCmd + str(ch) + "," + parser['channels']['p' + str(ch)] + ","
      channelform.channel_freq = parser['channels']['f' + str(ch)]
  #    configCmd = configCmd + parser['channels']['f' + str(ch)] + ","
      channellistform.channels.append_entry(channelform)
 #    send_to_mainctl(configCmd,1);
     if(statusCheck == True):
        send_channel_config()
        theStatus = "OK"
     else:
        theStatus = theStatus + " NOT SAVED"

   print("return to desetup")
   return render_template('desetup.html',
	      ringbufferPath = ringbufferPath, channelcount = channelcount,
          form = form, status = theStatus,
          channellistform = channellistform)

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


# stop propagation monitoring for FT8
def stopprop():
  send_to_mainctl(STOP_FT8_COLL,0)
  thePropStatus = 0
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
  band = []
 # print("Entering _/ft8list")
  parser = configparser.ConfigParser(allow_no_value=True)
  parser.read('config.ini')
  for i in range(7):
    ia = "ft8" + str(i) + "f"
    ib = "ftant" + str(i)
    if(parser['settings'][ib] != "Off"):
      band.append(parser['settings'][ia])
    #  print("ft8 band list=" + band[i])

  try:
    plist = []
    for fno in range(len(band)):
# TODO: following needs to come from configuration
     fname = '/mnt/RAM_disk/FT8/decoded' + str(fno) +'.txt'
  #   print("checking file",fname)
     f = open(fname,"r")
    # print("ft8list" + len(f.readlines()))
     plist.append(len(f.readlines()))
 #    print(plist)
     f.close()
      
# here we build a JSON string to populate the FT8 panel
    ft8string = '{'
    ft8string = ft8string + '"0":"MHz  spots",'
    for i in range(len(band)):
     pval = str(plist[i])
     ft8string = ft8string + '"' + str(i+1) + '":"' +  \
       str(band[i]) + ' - ' + pval + ' ",'

    ft8string = ft8string + '"end":" "}'
  #  print("ft8string= " , ft8string)
  except Exception as ex:
   # print(ex)
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

