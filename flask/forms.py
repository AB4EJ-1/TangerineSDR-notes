from flask_wtf import FlaskForm
from wtforms import Form, TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField

from wtforms import validators, ValidationError


class MainControlForm(FlaskForm):
  mode = SelectField('Mode', choices = [('snapshotter','Snapshotter'),
     ('ringbuffer','Ringbuffer'), ('firehose','Firehose')])
#  submit = SubmitField("Set Mode")
  startDC = SubmitField("Start Data Collection")
  stopDC = SubmitField("Stop Data Collection")

class ChannelControlForm(FlaskForm):
  antennaport0 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport1 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport2 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport3 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport4 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport5 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport6 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport7 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport8 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport9 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport10 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport11 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport12 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport13 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport14 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport15 = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  

class ThrottleControlForm(FlaskForm):
  throttle = SelectField('Bandwidth (bits/sec)', choices =
    [('Unlimited', 'Unlimited'),
     ('10 kbps',  '10k'),
     ('100 kbps', '100k'),
     ('1 Mbps',   '1M'),
     ('10 Mbps',  '10M'),
     ('100 Mbps', '100M')])
  submit = SubmitField("Set Bandwidth")
  
