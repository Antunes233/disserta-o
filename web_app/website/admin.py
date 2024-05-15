from django.contrib import admin
from .models import Doctor, Pacient, Sessions

# Register your models here.


class DoctorAdmin(admin.ModelAdmin):
    list_display = ("name", "id_number")


class PacientAdmin(admin.ModelAdmin):
    list_display = ("name", "pacient_number")

class SessionsAdmin(admin.ModelAdmin):
    list_display = ("session_id", "date", "Pacient")

admin.site.register(Doctor, DoctorAdmin)
admin.site.register(Pacient, PacientAdmin)
admin.site.register(Sessions, SessionsAdmin)
