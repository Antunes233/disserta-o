from django.urls import re_path
from . import consumers

websocket_urlpatterns = [
    re_path(r'ws/patientsession/(?P<patient_id>\d+)/$', consumers.PatientInfoConsumer.as_asgi()),
]
