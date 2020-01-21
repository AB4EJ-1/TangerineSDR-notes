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

@app.route("/desetup2",methods=['POST','GET'])
def members():
   return render_template('desetup.html')

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
         print("F: end of control loop; theStatus=", theStatus)
         form.destatus = theStatus
         form.dataStat = theDataStatus
         return render_template('tangerine.html', form = form)


@app.route("/restart")
def restart():
   global theStatus, theDataStatus
   print("F: restart")
   returned_value = os.system("killall -9 main")
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
   print("F: reached DE setup")
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
   if request.method == 'GET':
     ringbufferPath = parser['settings']['ringbuffer_path']

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
     ant4 =     parser['settings']['ant4']
     ch4f =     parser['settings']['ch4f']
     ch4b =     parser['settings']['ch4b']
     ant5 =     parser['settings']['ant5']
     ch5f =     parser['settings']['ch5f']
     ch5b =     parser['settings']['ch5b']
     ant6 =     parser['settings']['ant6']
     ch6f =     parser['settings']['ch6f']
     ch6b =     parser['settings']['ch6b']
     ant7 =     parser['settings']['ant7']
     ch7f =     parser['settings']['ch7f']
     ch7b =     parser['settings']['ch7b']
     ant8 =     parser['settings']['ant8']
     ch8f =     parser['settings']['ch8f']
     ch8b =     parser['settings']['ch8b']
     ant9 =     parser['settings']['ant9']
     ch9f =     parser['settings']['ch9f']
     ch9b =     parser['settings']['ch9b']
     ant10 =     parser['settings']['ant10']
     ch10f =     parser['settings']['ch10f']
     ch10b =     parser['settings']['ch10b']
     ant11 =     parser['settings']['ant11']
     ch11f =     parser['settings']['ch11f']
     ch11b =     parser['settings']['ch11b']
     ant12 =     parser['settings']['ant12']
     ch12f =     parser['settings']['ch12f']
     ch12b =     parser['settings']['ch12b']
     ant13 =     parser['settings']['ant13']
     ch13f =     parser['settings']['ch13f']
     ch13b =     parser['settings']['ch13b']
     ant14 =     parser['settings']['ant14']
     ch14f =     parser['settings']['ch14f']
     ch14b =     parser['settings']['ch14b']
     ant15 =     parser['settings']['ant15']
     ch15f =     parser['settings']['ch15f']
     ch15b =     parser['settings']['ch15b']
     print("F: ringbufferPath=",ringbufferPath)
     return render_template('desetup.html',
	  ringbufferPath = ringbufferPath,
      ant0 = ant0 , ch0f = ch0f, ch0b = ch0b,
	  ant1 = ant1 , ch1f = ch1f, ch1b = ch1b,
      ant2 = ant2 , ch2f = ch2f, ch2b = ch2b,
	  ant3 = ant3,  ch3f = ch3f, ch3b = ch3b,
	  ant4 = ant4,  ch4f = ch4f, ch4b = ch4b,
	  ant5 = ant5,  ch5f = ch5f, ch5b = ch5b,
	  ant6 = ant6,  ch6f = ch6f, ch6b = ch6b,
	  ant7 = ant7,  ch7f = ch7f, ch7b = ch7b,
	  ant8 = ant7,  ch8f = ch8f, ch8b = ch8b,
	  ant9 = ant9,  ch9f = ch9f, ch9b = ch9b,
	  ant10 = ant10,  ch10f = ch10f, ch10b = ch10b,
	  ant11 = ant11,  ch11f = ch11f, ch11b = ch11b,
	  ant12 = ant12,  ch12f = ch12f, ch12b = ch12b,
	  ant13 = ant13,  ch13f = ch13f, ch13b = ch13b,
	  ant14 = ant14,  ch14f = ch14f, ch14b = ch14b,
	  ant15 = ant15,  ch15f = ch15f, ch15b = ch15b )

   if request.method == 'POST':
     result = request.form
     print("F: result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("F: CANCEL")
     else:
       print("F: result of DEsetup post =")
       ringbufferPath = ""

       parser.set('settings', 'ringbuffer_path', result.get('ringbufferPath'))

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
       parser.set('settings', 'ant4',            str(result.get('ch4a')))
       parser.set('settings', 'ch4f',            str(result.get('ch4f')))
       parser.set('settings', 'ch4b',            str(result.get('ch4b')))
       parser.set('settings', 'ant5',            str(result.get('ch5a')))
       parser.set('settings', 'ch5f',            str(result.get('ch5f')))
       parser.set('settings', 'ch5b',            str(result.get('ch5b')))
       parser.set('settings', 'ant6',            str(result.get('ch6a')))
       parser.set('settings', 'ch6f',            str(result.get('ch6f')))
       parser.set('settings', 'ch6b',            str(result.get('ch6b')))
       parser.set('settings', 'ant7',            str(result.get('ch7a')))
       parser.set('settings', 'ch7f',            str(result.get('ch7f')))
       parser.set('settings', 'ch7b',            str(result.get('ch7b')))
       parser.set('settings', 'ant8',            str(result.get('ch8a')))
       parser.set('settings', 'ch8f',            str(result.get('ch8f')))
       parser.set('settings', 'ch8b',            str(result.get('ch8b')))
       parser.set('settings', 'ant9',            str(result.get('ch9a')))
       parser.set('settings', 'ch9f',            str(result.get('ch9f')))
       parser.set('settings', 'ch9b',            str(result.get('ch9b')))
       parser.set('settings', 'ant10',            str(result.get('ch10a')))
       parser.set('settings', 'ch10f',            str(result.get('ch10f')))
       parser.set('settings', 'ch10b',            str(result.get('ch10b')))
       parser.set('settings', 'ant11',            str(result.get('ch11a')))
       parser.set('settings', 'ch11f',            str(result.get('ch11f')))
       parser.set('settings', 'ch11b',            str(result.get('ch11b')))
       parser.set('settings', 'ant12',            str(result.get('ch12a')))
       parser.set('settings', 'ch12f',            str(result.get('ch12f')))
       parser.set('settings', 'ch12b',            str(result.get('ch12b')))
       parser.set('settings', 'ant13',            str(result.get('ch13a')))
       parser.set('settings', 'ch13f',            str(result.get('ch13f')))
       parser.set('settings', 'ch13b',            str(result.get('ch13b')))
       parser.set('settings', 'ant14',            str(result.get('ch14a')))
       parser.set('settings', 'ch14f',            str(result.get('ch14f')))
       parser.set('settings', 'ch14b',            str(result.get('ch14b')))
       parser.set('settings', 'ant15',            str(result.get('ch15a')))
       parser.set('settings', 'ch15f',            str(result.get('ch15f')))
       parser.set('settings', 'ch15b',            str(result.get('ch15b')))
     
       fp = open('config.ini','w')
       parser.write(fp)
       fp.close()

     ringbufferPath = parser['settings']['ringbuffer_path']

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
     ant4 =     parser['settings']['ant4']
     ch4f =     parser['settings']['ch4f']
     ch4b =     parser['settings']['ch4b']
     ant5 =     parser['settings']['ant5']
     ch5f =     parser['settings']['ch5f']
     ch5b =     parser['settings']['ch5b']
     ant6 =     parser['settings']['ant6']
     ch8f =     parser['settings']['ch6f']
     ch6b =     parser['settings']['ch6b']
     ant7 =     parser['settings']['ant7']
     ch7f =     parser['settings']['ch7f']
     ch7b =     parser['settings']['ch7b']
     ant8 =     parser['settings']['ant8']
     ch8f =     parser['settings']['ch8f']
     ch8b =     parser['settings']['ch8b']
     ant9 =     parser['settings']['ant9']
     ch9f =     parser['settings']['ch9f']
     ch9b =     parser['settings']['ch9b']
     ant10 =     parser['settings']['ant10']
     ch10f =     parser['settings']['ch10f']
     ch10b =     parser['settings']['ch10b']
     ant11 =     parser['settings']['ant11']
     ch11f =     parser['settings']['ch11f']
     ch11b =     parser['settings']['ch11b']
     ant12 =     parser['settings']['ant12']
     ch12f =     parser['settings']['ch12f']
     ch12b =     parser['settings']['ch12b']
     ant13 =     parser['settings']['ant13']
     ch13f =     parser['settings']['ch13f']
     ch13b =     parser['settings']['ch13b']
     ant14 =     parser['settings']['ant14']
     ch14f =     parser['settings']['ch14f']
     ch14b =     parser['settings']['ch14b']
     ant15 =     parser['settings']['ant15']
     ch15f =     parser['settings']['ch15f']
     ch15b =     parser['settings']['ch15b']
     print("F: ringbufferPath=",ringbufferPath)
     return render_template('desetup.html',
	  ringbufferPath = ringbufferPath,
      ant0 = ant0 , ch0f = ch0f, ch0b = ch0b,
	  ant1 = ant1 , ch1f = ch1f, ch1b = ch1b,
      ant2 = ant2 , ch2f = ch2f, ch2b = ch2b,
	  ant3 = ant3,  ch3f = ch3f, ch3b = ch3b,
	  ant4 = ant4,  ch4f = ch4f, ch4b = ch4b,
	  ant5 = ant5,  ch5f = ch5f, ch5b = ch5b,
	  ant6 = ant6,  ch6f = ch6f, ch6b = ch6b,
	  ant7 = ant7,  ch7f = ch7f, ch7b = ch7b,
	  ant8 = ant7,  ch8f = ch8f, ch8b = ch8b,
	  ant9 = ant9,  ch9f = ch9f, ch9b = ch9b,
	  ant10 = ant10,  ch10f = ch10f, ch10b = ch10b,
	  ant11 = ant11,  ch11f = ch11f, ch11b = ch11b,
	  ant12 = ant12,  ch12f = ch12f, ch12b = ch12b,
	  ant13 = ant13,  ch13f = ch13f, ch13b = ch13b,
	  ant14 = ant14,  ch14f = ch14f, ch14b = ch14b,
	  ant15 = ant15,  ch15f = ch15f, ch15b = ch15b )

@app.route("/startcollection")
def startcoll():
  form = MainControlForm()
  global theStatus, theDataStatus
  print("F: Start Data Collection command")
  
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

@app.route("/stopcollection")
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
   parser = configparser.ConfigParser(allow_no_value=True)
   parser.read('config.ini')
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
      smtppw = smtppw)

   if request.method == 'POST':
     result = request.form
     print("F: result=", result.get('csubmit'))
     if result.get('csubmit') == "Discard Changes":
       print("F: CANCEL")

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

