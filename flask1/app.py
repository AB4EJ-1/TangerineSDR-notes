from flask import Flask, render_template, request, url_for
#from flask.ext.wtf import Form
from wtforms import Form, TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField, StringField, DecimalField, BooleanField, FieldList, FormField, FloatField
from wtforms import StringField, FieldList, FormField, SelectField
from wtforms.validators import DataRequired
from werkzeug.datastructures import MultiDict

from wtforms import validators, ValidationError

app = Flask(__name__)
app.config['SECRET_KEY']='asdfjlkghdsf'

# normally student data is read in from a file uploaded, but for this demo we use dummy data
student_info=[("123","Bob Jones","2.5"),("234","Peter Johnson","5.0"),("345","Carly Everett","10.0"),
              ("456","Josephine Edgewood","15.0"),("567","Pat White","20.0"),("678","Jesse Black","25.5")]

class FileUploadForm(Form):
    pass

class StudentForm(Form):
#    student_id = StringField('Student ID', validators = [DataRequired()])
 #   student_name = StringField('Student Name', validators = [DataRequired()])
    student_id = StringField('Student ID')
    student_name = StringField('Student Name')
    student_ant = SelectField('AntennaPort',choices = [('0','0'),('1','1')])
    student_freq = FloatField('CH 0 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
    student_rate = SelectField('SampleRate',choices=[('4000','4000'),('8000','8000'),('12000','12000'),('48000','48000'),('96000','96000'),('120000','120000')])

class AddClassForm(Form):
 #   name = StringField('classname', validators=[DataRequired()])
    name = StringField('classname')
    day = SelectField('classday', 
                      choices=[(1,"Monday"),(2,"Tuesday"),(3,"Wednesday"),(4,"Thursday"),(5,"Friday")],
                      coerce=int)

    students = FieldList(FormField(StudentForm), min_entries = 4) # show at least 5 blank fields by default

@app.route('/', methods=['GET', 'POST'])
def addclass():
  fileform = FileUploadForm()
  classform = AddClassForm()

  if request.method == 'POST':
    # Check which 'submit' button was called to validate the correct form
#    if 'addclass' in request.form and classform.validate_on_submit():
    if 'addclass' in request.form and classform.validate():
        # Add class to DB - not relevant for this example.
        return redirect(url_for('addclass'))
    if not classform.validate():
      print("FORM ERR")
      print(classform.errors)

    if 'upload' in request.form :   # and fileform.validate_on_submit():
        # get the data file from the post - not relevant for this example.
        # overwrite the classform by populating it with values read from file
        print("populate form from file")
        classform = PopulateFormFromFile()
 #       print(classform.students)
        return render_template('addclass.html', classform=classform)

  return render_template('addclass.html', fileform=fileform, classform=classform)

def PopulateFormFromFile():
    classform = AddClassForm()

    # normally we would read the file passed in as an argument and pull data out, 
    # but let's just use the dummy data from the top of this file, and some hardcoded values
    classform.name.data = "Super Awesome Class"
    classform.day.data = 4 # Thursday

    # pop off any blank fields already in student info
    while len(classform.students) > 0:
        classform.students.pop_entry()

    for student_id, name, student_freq in student_info:
        studentform = StudentForm()
        studentform.student_freq = student_freq
        studentform.student_id = student_id
        studentform.student_name = name

        classform.students.append_entry(studentform)
  #      print(classform.students)
    return classform


if __name__ == '__main__':
    app.run(debug=True, port=5001)
