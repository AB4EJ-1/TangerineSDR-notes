from flask_wtf import FlaskForm
from wtforms import Form, TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField, StringField, DecimalField, BooleanField, FieldList, FormField, FloatField

from wtforms import validators, ValidationError


class MainControlForm(FlaskForm):
  mode = SelectField('Mode', choices = [('snapshotter','Snapshotter'),
     ('ringbuffer','Ringbuffer'), ('firehose','Firehose')])
#  submit = SubmitField("Set Mode")
  startDC = SubmitField("Start Data Collection")
  stopDC = SubmitField("Stop Data Collection")
  prop = SelectField('Type', choices = [('FT8','FT8'),('WSPR','WSPR')])
  startprop = SubmitField("Start Monitoring")
  stopprop  = SubmitField("Stop Monitoring")

class ChannelForm(Form):
  channel_ant = SelectField('AntennaPort',choices = [('0','0'),('1','1')])
  channel_freq = FloatField('CH_freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])

class ChannelListForm(Form):
#  channels = FieldList(FormField(ChannelForm), min_entries = 1)
  channels = FieldList(FormField(ChannelForm))

class ChannelControlForm(FlaskForm):
  channelcount = SelectField('ChannelCount',choices=[('1','1'),('2','2'),('3','3'),
          ('4','4'),('5','5'),('6','6'),('7','7'),('8','8'),('9','9'),('10','10'),('11','11'),
          ('12','12'),('13','13'),('14','14'),('15','15'),('`16','16')])
  channelrate = SelectField(u'Rate', coerce=int)
# temporary setup for flex form
  chp_setting = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport = []
  for i in range(0,2):
    antennaport.append(SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')]))
  pskindicator = BooleanField('Active')
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
  ch0f = DecimalField('CH 0 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch1f = DecimalField('CH 1 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch2f = DecimalField('CH 2 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch3f = DecimalField('CH 3 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch4f = DecimalField('CH 4 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch5f = DecimalField('CH 5 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch6f = DecimalField('CH 6 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch7f = DecimalField('CH 7 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch8f = DecimalField('CH 8 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch9f = DecimalField('CH 9 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch10f = DecimalField('CH 10 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch11f = DecimalField('CH 11 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch12f = DecimalField('CH 12 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch13f = DecimalField('CH 13 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch14f = DecimalField('CH 14 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch15f = DecimalField('CH 15 freq',[validators.Optional(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])
  ch0b = DecimalField('CH 0 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch1b = DecimalField('CH 1 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch2b = DecimalField('CH 2 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch3b = DecimalField('CH 3 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch4b = DecimalField('CH 4 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch5b = DecimalField('CH 5 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch6b = DecimalField('CH 6 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch7b = DecimalField('CH 7 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch8b = DecimalField('CH 8 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch9b = DecimalField('CH 9 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch10b = DecimalField('CH 10 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch11b = DecimalField('CH 11 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch12b = DecimalField('CH 12 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch13b = DecimalField('CH 13 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch14b = DecimalField('CH 14 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  ch15b = DecimalField('CH 15 bw',[validators.Optional(),validators.NumberRange(min=0.001, max = 1000, message=(u'Bandwidth out of range'))])
  
class ThrottleControlForm(FlaskForm):
  throttle = SelectField('Bandwidth (bits/sec)', choices =
    [('Unlimited', 'Unlimited'),
     ('10 kbps',  '10k'),
     ('100 kbps', '100k'),
     ('1 Mbps',   '1M'),
     ('10 Mbps',  '10M'),
     ('100 Mbps', '100M')])
  submit = SubmitField("Set Bandwidth")

class CallsignForm(FlaskForm):
  submit = SubmitField('Save callsigns')

class ServerControlForm(FlaskForm):
  emailto = StringField('email from',[
    validators.Length(min=6, message=(u'Too short for email addr')),
    validators.Email(message=(u'Email_from Not a valid address')),
    validators.DataRequired(message=(u'Email addr required'))])
  emailfrom = StringField('email from',[validators.Email(message=(u'Email_from Not a valid address')),
           validators.DataRequired(message=(u'Email addr required'))])
  smtpport = IntegerField('smtpport',[validators.NumberRange(min=10, max=66000,message=(u'SMTP port out of range')),validators.DataRequired(message=(u'SMTP port required'))])
  smtpsvr = StringField('smtpsver',[validators.DataRequired(message=(u'SMTP server address required'))])
  smtpuid = StringField('smtpuid',[validators.DataRequired(message=(u'SMTP User ID required'))])



  
