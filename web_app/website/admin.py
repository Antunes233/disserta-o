from django.contrib import admin
from .models import Doctor, Patient, Sessions

# Register your models here.


class DoctorAdmin(admin.ModelAdmin):
    list_display = ("name", "id_number")

class PatientAdmin(admin.ModelAdmin):
    list_display = ("name", "patient_number")

class SessionsAdmin(admin.ModelAdmin):
    list_display = ("session_id", "date", "Patient")

admin.site.register(Doctor, DoctorAdmin)
admin.site.register(Patient, PatientAdmin)
admin.site.register(Sessions, SessionsAdmin)
