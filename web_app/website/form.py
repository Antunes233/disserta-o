from django import forms
from .models import Patient
from django.contrib.auth.models import User


class PatientForm(forms.ModelForm):
    class Meta:
        model = Patient
        fields = ['name', 'phone_number', 'height', 'weight', 'age', 'gender']