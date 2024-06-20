from django.contrib import admin
from .models import Doctor, Patient, Sessions
from django.http import HttpResponseRedirect
from django.urls import reverse
from django.urls import path
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


class CustomAdminSite(admin.AdminSite):
    def get_urls(self):
        urls = super().get_urls()
        custom_urls = [
            path('custom/', self.admin_view(self.custom_admin_view), name='custom_admin_page'),
        ]
        return custom_urls + urls

    def custom_admin_view(self, request):
        return HttpResponseRedirect(reverse('custom_admin_page'))

admin_site = CustomAdminSite(name='custom_admin')