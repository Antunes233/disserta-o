from django.urls import path, re_path
from . import views
from django.contrib.auth import views as auth_views
from django.contrib import admin



urlpatterns = [
    path('logout/', auth_views.LogoutView.as_view(), name='logout'),
    path("", views.Login, name="login"),
    path("details/<int:id>", views.Details, name="details"),
    path("details/patient/<int:id>", views.Patientinfo, name="pinfo"),
    path("details/patient/<int:id>/new_session", views.PatientSession, name="newsession"),
    path("details/patient/<int:id>/session_<int:session_id>", views.SessionReview, name="sessionreview"),
]
