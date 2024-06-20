from django.contrib.postgres.fields import ArrayField
from django.db import models
import random
from django.utils import timezone

# Create your models here.


class Doctor(models.Model):
    name = models.CharField(max_length=100)
    id_number = models.IntegerField(null=True)
    phone_number = models.IntegerField(null=True)
    password = models.CharField(max_length=100, null=True)
    last_login = models.DateTimeField(null=True)

    def __str__(self):
        return f"{self.name} {self.id_number}"


class Patient(models.Model):
    name = models.CharField(max_length=100)
    patient_number = models.IntegerField(null=True)
    phone_number = models.IntegerField(null=True)
    height = models.FloatField(null=True)
    weight = models.FloatField(null=True)
    age = models.IntegerField(null=True)
    gender = models.CharField(max_length=10, null=True)
    doc = models.ForeignKey(Doctor, on_delete=models.CASCADE, null=True)
    session_num = models.IntegerField(null=True)

    def __str__(self):
        return f"{self.name} {self.patient_number}"


class Sessions(models.Model):

    Patient = models.ForeignKey(Patient,on_delete=models.CASCADE)
    session_id = models.IntegerField(null=True)
    session_results_r = models.CharField(max_length=5000,null=True)
    session_results_l = models.CharField(max_length=5000,null=True)
    date = models.DateTimeField(default=timezone.now)
    notes = models.TextField(null=True, blank=True)
    session_time = models.IntegerField(null=True)

    def __str__(self):
        return f"{self.session_id} {self.date} {self.Patient}"