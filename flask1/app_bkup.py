from flask import Flask, render_template, request, url_for
#from flask.ext.wtf import Form
from wtforms import Form, TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField, StringField, DecimalField, BooleanField, FieldList, FormField
from wtforms import StringField, FieldList, FormField, SelectField
from wtforms.validators import DataRequired
from werkzeug.datastructures import MultiDict

app = Flask(__name__)
app.config['SECRET_KEY']='asdfjlkghdsf'

# normally student data is read in from a file uploaded, but for this demo we use dummy data
student_info=[("123","Bob Jones"),("234","Peter Johnson"),("345","Carly Everett"),
              ("456","Josephine Edgewood"),("567","Pat White"),("678","Jesse Black")]

class FileUploadForm(Form):
    pass

class StudentForm(Form):
    student_id = StringField('Student ID', validators = [DataRequired()])
    student_name = StringField('Student Name', validators = [DataRequired()])

class AddClassForm(Form):
    name = StringField('classname', validators=[DataRequired()])
    day = SelectField('classday', 
                      choices=[(1,"Monday"),(2,"Tuesday"),(3,"Wednesday"),(4,"Thursday"),(5,"Friday")],
                      coerce=int)

    students = FieldList(FormField(StudentForm), min_entries = 5) # show at least 5 blank fields by default

@app.route('/', methods=['GET', 'POST'])
def addclass():
    fileform = FileUploadForm()
    classform = AddClassForm()

    # Check which 'submit' button was called to validate the correct form
    if 'addclass' in request.form and classform.validate_on_submit():
        # Add class to DB - not relevant for this example.
        return redirect(url_for('addclass'))

    if 'upload' in request.form and fileform.validate_on_submit():
        # get the data file from the post - not relevant for this example.
        # overwrite the classform by populating it with values read from file
        classform = PopulateFormFromFile()
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

    for student_id, name in student_info:
        # either of these ways have the same end result.
        #
        # studentform = StudentForm()
        # studentform.student_id.data = student_id
        # studentform.student_name.data = name
        #
        # OR
  #      student_data = MultiDict([('student_id',student_id), ('student_name',name)])
  #      studentform = StudentForm(student_data)

        studentform = StudentForm()    
        studentform.student_id = student_id
        studentform.student_name = name

        classform.students.append_entry(studentform)

    return classform


if __name__ == '__main__':
    app.run(debug=True, port=5001)
