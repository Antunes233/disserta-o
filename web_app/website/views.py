from django.shortcuts import render, redirect
from django.urls import reverse
from django.http import HttpResponse, HttpResponseRedirect
from django.template import loader
from .models import Doctor, Patient, Sessions
from django.contrib.auth import authenticate, login
from django.contrib.auth.forms import AuthenticationForm
from django.contrib.auth.decorators import login_required
import calendar
from calendar import HTMLCalendar
from calendar import monthrange
from datetime import datetime, timedelta
import torch
import matplotlib.pyplot as plt
import os
from django.conf import settings
from .models_ml.knee_model import KneeFlexionModel
import numpy as np
from .utilities.plot import generate_plot, generate_expected_curve
from scipy.signal import savgol_filter
import json
from django.http import JsonResponse
import paho.mqtt.client as mqtt
import web_app.settings as settings
from django.template.loader import render_to_string
from .mqtt.mqtt import disconnect
from io import BytesIO
import base64
from .form import PatientForm
import logging
from django.utils import timezone
from .backends import DoctorBackend
from django.contrib.auth.models import User
import random
from django.contrib.admin.views.decorators import staff_member_required

# Create your views here.
BASE_DIR = settings.BASE_DIR
model_path = os.path.join(BASE_DIR, 'website', 'models_ml', 'KneeFlexionModel.pth')
model = KneeFlexionModel()
model.load_state_dict(torch.load(model_path))


# Initialize MQTT client
mqtt_client = mqtt.Client()

logger = logging.getLogger(__name__)


def custom_admin_page(request):
    return render(request, 'admin/custom_admin_page.html')

def Login(request):
    if request.method == "POST":
        username = request.POST["username"]
        password = request.POST["password"]

        # Use the custom authentication backend
        user = DoctorBackend().authenticate(request, username=username, password=password)
        if user is not None:
            login(request, user, backend='website.backends.DoctorBackend')
            # Debug statement to verify user is authenticated
            print(f"User {user.name} authenticated successfully.")
            doc = Doctor.objects.get(name=username, password=password)
            # Successful login, redirect to a different view or page
            return redirect("details/" + str(doc.id))
        else:
            # Invalid credentials, display an error message
            return render(request, "loginpage.html", {"error": "Invalid username or password"})

    # Render the login page
    return render(request, "loginpage.html")



def Details(request, id):
    try:
        doc = Doctor.objects.get(id=id)
        patients = Patient.objects.filter(doc=doc)
        currentMonth = timezone.now().month
        currentYear = timezone.now().year
        day = timezone.now().day
        doc.last_login = timezone.now()
        doc.save()
        calendar = HTMLCalendar().formatmonth(
            currentYear, currentMonth).replace(
                '<td ', '<td  width="50" height="50"')


        if request.method == 'POST':
            form = PatientForm(request.POST)
            if form.is_valid():
                patient = form.save(commit=False)
                patient.doc = doc # Associate the patient with the doctor
                patient.patient_number = random.randint(1, 9999999)
                patient.save()
                return redirect('/details/patient/'+str(patient.id))
            else:
                print(form.errors)  # Print form errors to the console for debugging
        else:
            form = PatientForm()

        context = {
            "doc": doc,
            "patients": patients,
            "calendar": calendar,
            "form": form,

        }
        template = loader.get_template("details.html")
        return HttpResponse(template.render(context, request))
    except Exception as e:
        logger.error(f"Error loading details: {e}")
        return HttpResponse(render(request, "error.html"))



def Patientinfo(request, id):
    patient = Patient.objects.get(id=id)
    doc = Doctor.objects.get(id=patient.doc_id)
    template = loader.get_template("patientinfo.html")
    session = Sessions.objects.filter(Patient_id=id)
    if patient.gender == 'Male':
        gender = 1
    else:
        gender = 0
    with torch.no_grad():
        knee_angle_curve = model(torch.tensor([[patient.age, gender, patient.weight, patient.height]]))
    smoothed_curve = savgol_filter(knee_angle_curve[0].cpu().detach().numpy(), 15, 4)
    # plot = generate_plot(list(range(100)), smoothed_curve)

    if request.method == 'POST':
        if 'go_home' in request.POST:
            return redirect('/details/' + str(doc.id))

    # mqtt_client.disconnect(client)

    context = {
        "patient": patient,
        # "plot_data": plot,
        "sessions": session,
    }
    return HttpResponse(template.render(context, request))


#-----------------------------------------------------------------------------------------------------------------
mqtt_client = mqtt.Client()

mqtt_flag = False
final_data_r = ""
final_data_l = ""
qos = 0
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe("django/gait_values_left", qos=1)
        client.subscribe("django/gait_values_right", qos=1)
    else:
        logger.error(f"Failed to connect to MQTT broker, return code {rc}")

def on_message(client, userdata, msg):
    global final_data_r, final_data_l

    if msg.topic == "django/gait_values_right":
        if msg.payload.decode() == 'End':
            print("Session ended for right leg")
            session = Sessions.objects.filter(session_status_r="Ongoing").last()
            session.session_results_r = final_data_r
            session.session_status_r = "Completed"
            session.save()

            publish(client, "Received", "django/confirm")
        else:
            if final_data_r == "":
                final_data_r = msg.payload.decode()
            else:
                final_data_r = final_data_r + "," + msg.payload.decode()

    elif msg.topic == "django/gait_values_left":
        if msg.payload.decode() == 'End':
            print("Session ended for left leg")
            session = Sessions.objects.filter(session_status_l="Ongoing").last()
            session.session_results_l = final_data_l
            session.session_status_l = "Completed"
            session.save()

        else:
            if final_data_l == "":
                final_data_l = msg.payload.decode()
            else:
                final_data_l = final_data_l + "," + msg.payload.decode()


def publish(client, msg, topic):

        result = client.publish(topic, msg, qos=2,retain=False)
        # result: [0, 1]
        status = result[0]
        if status == 0:
            print(f"Send `{msg}` to topic `{topic}`")
        else:
            print(f"Failed to send message to topic {topic}")

mqtt_client.on_connect = on_connect
def PatientSession(request, id):
    message = None
    global mqtt_client
    patient = Patient.objects.get(id=id)
    template = loader.get_template("patientsession.html")
    session_going = None
    end_session = None
    notification = None
    notification_pop = None

    mqtt_client.on_message = on_message
    if request.method == "POST":
        global mqtt_flag
        # global mqtt_client
        if 'botão_sessão' in request.POST:
            if mqtt_flag == False:
                if patient.session_num == None:
                    num = 1
                else:
                    num = int(patient.session_num)
                    num = num + 1

                patient.session_num = num
                patient.save()
                session = Sessions.objects.create(Patient=patient, session_id = num)
                mqtt_flag = True
                mqtt_client.connect(settings.MQTT_SERVER, settings.MQTT_PORT)
                mqtt_client.loop_start()

                publish(mqtt_client, "Begin", "django/confirm")
                mqtt_client.on_message = on_message
                # Start MQTT client loop (this function will block)
                message = "Session started"
            else:
                notification = "You already have a session going"


        if 'progress' in request.POST:
            if mqtt_flag == True:
                session_data = Sessions.objects.get(Patient=patient, session_id=patient.session_num)
                if session_data.session_status_r == "Completed" and session_data.session_status_l == "Completed":
                    end_session = "Session Completed"
                    mqtt_flag = False
                else:
                    session_going = "Waiting for data..."
            else:
                notification = "No progress to check"

        if 'end_session' in request.POST:
            if mqtt_flag == True:
                if mqtt_client.is_connected():
                    publish(mqtt_client,"End", "django/confirm")
                    notification = "Session is ending... please wait for data retrieval"
                else:
                    mqtt_client.connect(settings.MQTT_SERVER, settings.MQTT_PORT)
                    if mqtt_client.is_connected():
                        publish(mqtt_client,"End", "django/confirm")
                        notification = "Session is ending... please wait for data retrieval"
                    else:
                        notification = "MQTT client is not connected"
            else:
                notification = "No session in place"


        if 'go_back' in request.POST:
            if mqtt_flag == False:
                mqtt_client.loop_stop()
                disconnect(mqtt_client)
                return HttpResponseRedirect('/details/patient/'+str(id))
            else:
                notification = "Wait for session to end"

    context = {
        # "minutes": minutes,
        # "seconds": seconds,
        "patient": patient,
        "message": message,
        "end_message": session_going,
        "end_session": end_session,
        "notification": notification,
        "notification_pop": notification_pop,
    }
    return HttpResponse(template.render(context, request))


#-------------------------------------------------------------------------------------------------------------------------------



def SessionReview(request, id, session_id):
    session = Sessions.objects.get(Patient = id, id=session_id)
    patient = Patient.objects.get(id=id)
    template = loader.get_template("sessionreview.html")
    data_r = session.session_results_r.strip('[]').split(',')
    data_l = session.session_results_l.strip('[]').split(',')
    data_list_r = [float(i) for i in data_r]
    data_list_l = [float(i) for i in data_l]

    plot = generate_plot(x1=list(np.arange(0,len(data_list_r)/100,((len(data_list_r)/100)/len(data_list_r)))),
                         x2=list(np.arange(0,len(data_list_l)/100,((len(data_list_l)/100)/len(data_list_l)))),
                         y_r=data_list_r,y_l= data_list_l)

    if patient.gender == 'Male':
        gender = 1
    else:
        gender = 0
    with torch.no_grad():
        knee_angle_curve = model(torch.tensor([[patient.age, gender, patient.weight, patient.height]]))
    smoothed_curve = savgol_filter(knee_angle_curve[0].cpu().detach().numpy(), 15, 4)
    plot_expected = generate_expected_curve(list(range(100)), smoothed_curve)

    if request.method == "POST":
        if 'go_back' in request.POST:
            return HttpResponseRedirect('/details/patient/'+str(id))

    if request.method == 'POST':
        session.notes = request.POST.get('session_notes', '')
        session.save()

    context = {
        "session": session,
        "patient": patient,
        "plot_data": plot,
        "expected_curve": plot_expected,
    }
    return HttpResponse(template.render(context, request))