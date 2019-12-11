from flask import Flask, flash, redirect, render_template, request, session, abort
import socket
app = Flask(__name__)

#@app.route("/")
#def index():
#    return "Index!"
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

@app.route("/")
def sdr():
   return render_template('tangerine.html',result = theStatus)

@app.route("/config")
def config():
   return render_template('config.html')

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
#	from waitress import serve
#	serve(app, host = "0.0.0.0", port=5000)
