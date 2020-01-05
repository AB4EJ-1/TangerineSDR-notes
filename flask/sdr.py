from flask import Flask, render_template, request, flash
from forms import ContactForm

from forms import MainControlForm

app = Flask(__name__)
app.secret_key = 'development key'



@app.route('/contact', methods = ['GET', 'POST'])
def contact():
   form = ContactForm()

   if request.method == 'POST':
      print("POST received")
      if form.validate() == False:
         flash('All fields are required.')
         return render_template('contact.html', form = form)
      else:
         print(" ********** lang = ", form.language.data)
         return render_template('contact.html', form = form)
      
   elif request.method == 'GET':
         return render_template('contact.html', form = form)

# Here is the home page
@app.route("/", methods = ['GET', 'POST'])
def sdr():
   form = MainControlForm()
   global theStatus;
#   theStatus = check_status_once()
#   print("WEB status ", theStatus)
#   print('mode set to:',form.mode.data)
   if request.method == 'GET':
     
     return render_template('tangerine.html',form = form)
   if request.method == 'POST':
      if form.validate() == False:
         flash('All fields are required.')
         return render_template('tangerine.html', form = form)
      else:
         result = request.form
#         print("MODE SET TO ",result.get('form.mode'))
         print('mode set to:',form.mode.data)
         return render_template('tangerine.html', form = form)

if __name__ == '__main__':
   app.run(debug = True)
