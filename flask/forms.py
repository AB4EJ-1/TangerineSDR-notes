from flask_wtf import FlaskForm
from wtforms import Form, TextField, IntegerField, TextAreaField, SubmitField, RadioField, SelectField, StringField, DecimalField, BooleanField, FieldList, FormField, FloatField

from wtforms import validators, ValidationError

class MainControlForm(FlaskForm):
  class Meta:
    csrf = True
    csrf_time_limit = None
  mode = SelectField('Mode', choices = [('snapshotter','Snapshotter'),
     ('ringbuffer','Ringbuffer'), ('snapring','SnapRing'), ('firehoseR','FirehoseRemote'),
     ('firehoseL','FirehoseLocal')])
  modeR = BooleanField('Ringbuffer',default=False)
  modeS = BooleanField('Snapshotter',default=False)
  modeF = BooleanField('Firehose(upload)',default=False)
  startDC = SubmitField("Start Data Collection")
  stopDC = SubmitField("Stop Data Collection")
  prop = SelectField('Type', choices = [('FT8','FT8'),('WSPR','WSPR')])
  startprop = SubmitField("Start Monitoring")
  stopprop  = SubmitField("Stop Monitoring")

class ChannelForm(Form):
  channel_ant = SelectField('AntennaPort',choices = [('0','0'),('1','1')])
  channel_freq = FloatField('CH_freq',[validators.DataRequired(),validators.NumberRange(min=0.1, max = 54, message=(u'Freq out of range'))])

class ChannelListForm(Form):
  channels = FieldList(FormField(ChannelForm))

class ChannelControlForm(FlaskForm):
  channelcount = SelectField('ChannelCount',choices=[('1','1'),('2','2'),('3','3'),
          ('4','4'),('5','5'),('6','6'),('7','7'),('8','8'),('9','9'),('10','10'),('11','11'),
          ('12','12'),('13','13'),('14','14'),('15','15'),('`16','16')])
  maxRingbufsize = SelectField('RingbufMax',choices=[('1MB','1MB'),('10MB','10MB'),('50MB','50MB'),
          ('100MB','100MB'),('500MB','500MB'),('1GB','1GB'),('10GB','10GB'),('50GB','50GB'),
          ('100GB','100GB'),('500GB','500GB'),('1TB','1TB'),('2TB','2TB'),('4TB','4TB')])
  channelrate = SelectField(u'Rate', coerce=int)
# temporary setup for flex form
  chp_setting = SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')])
  antennaport = []
  for i in range(0,2):
    antennaport.append(SelectField('AntennaPort',choices = [('Off','Off'),('0','0'),('1','1')]))
  pskindicator = BooleanField(default=False)
  wsprindicator = BooleanField(default=True)
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
  ft80f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ft81f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ft82f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ft83f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ft84f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ft85f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ft86f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ft87f = SelectField('CH 0 freq',choices=[('1.84','1.84'),('3.573','3.573'),('7.074','7.074'),('10.136','10.136'),('14.074','14.074'),('18.1','18.1'),('21.074','21.074'),('24.915','24.915'),('28.074','28.074'),('50.313','50.313')])
  ws0f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
  ws1f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
  ws2f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
  ws3f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
  ws4f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
  ws5f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
  ws6f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
  ws7f = SelectField('CH 0 freq',choices=[('0.4742','0.4742'),('1.8366','1.8366'),('3.5686','3.5686'),('5.2872','5.2872'),('5.3647','5.3647'),('7.0386','7.0386'),('10.1387','10.1387'),('14.0956','14.0956'),('18.1046','18.1046'),('21.0946','21.0946'),('24.9246','24.9246'),('28.1246','28.1246'),('50.293','50.293')])
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
     ('10000',  '10k'),
     ('100000', '100k'),
     ('1000000',   '1M'),
     ('10000000',  '10M'),
     ('100000000', '100M')])
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



  
