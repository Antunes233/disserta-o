from django import forms
from .models import Pacient
from django.contrib.auth.models import User

class PatientForm(forms.ModelForm):
    class Meta:
        model = Pacient
        fields = ['name', 'pacient_number', 'phone_number', 'height', 'weight', 'age', 'gender']