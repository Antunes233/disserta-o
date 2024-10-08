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
    path('get-data-points/<int:start>/<int:count>/<int:id>/<int:session_id>/', views.get_data_points, name='get_data_points'),
    path('get-expected-curve/<int:id>/', views.get_expected_curve, name='get_expected_curve'),
    path('get-data-between-indexes-right/<int:id>/<int:session_id>/<int:start_index>/<int:end_index>/', views.get_data_between_indexes_right, name='get_data_between_indexes_right'),
    path('get-data-between-indexes-left/<int:id>/<int:session_id>/<int:start_index>/<int:end_index>/', views.get_data_between_indexes_left, name='get_data_between_indexes_left'),
    path('get-all-data-points/<int:id>/<int:session_id>/', views.get_all_data_points, name='get_all_data_points'),
]
