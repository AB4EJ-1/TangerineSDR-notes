from flask_wtf import FlaskForm
from wtforms import Form, TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField

from wtforms import validators, ValidationError


class MainControlForm(FlaskForm):
  mode = SelectField('Mode', choices = [('snapshotter','Snapshotter'),
     ('ringbuffer','Ringbuffer'), ('firehose','Firehose')])
  submit = SubmitField("Set Mode")
  startDC = SubmitField("Start Data Collection")
  stopDC = SubmitField("Stop Data Collection")


class ThrottleControlForm(FlaskForm):
  throttle = SelectField('Bandwidth (bits/sec)', choices =
    [('Unlimited', 'Unlimited'),
     ('10 kbps',  '10k'),
     ('100 kbps', '100k'),
     ('1 Mbps',   '1M'),
     ('10 Mbps',  '10M'),
     ('100 Mbps', '100M')])
  submit = SubmitField("Set Bandwidth")
  
