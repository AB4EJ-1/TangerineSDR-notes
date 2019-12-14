from flask import Flask
app = Flask(__name__)

@app.route("/")
def hello():
    return "Hello World!"

if __name__ == "__main__":
	app.run()
#	from waitress import serve
#	serve(app, host = "0.0.0.0#", port=5000)
#	
