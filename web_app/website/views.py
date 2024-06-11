from django.shortcuts import render, redirect
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
from .utilities.plot import generate_plot
from scipy.signal import savgol_filter
import json
from django.http import JsonResponse
import paho.mqtt.client as mqtt
import web_app.settings as settings
from django.template.loader import render_to_string
from .mqtt.mqtt import on_connect, disconnect
from io import BytesIO
import base64
from .form import PatientForm
import logging
from django.utils import timezone
from .backends import DoctorBackend
from django.contrib.auth.models import User
import random

# Create your views here.
BASE_DIR = settings.BASE_DIR
model_path = os.path.join(BASE_DIR, 'website', 'models_ml', 'KneeFlexionModel.pth')
model = KneeFlexionModel()
model.load_state_dict(torch.load(model_path))


# Initialize MQTT client
mqtt_client = mqtt.Client()

logger = logging.getLogger(__name__)


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
    plot = generate_plot(list(range(100)), smoothed_curve)

    if request.method == 'POST':
        if 'go_home' in request.POST:
            return redirect('/details/' + str(doc.id))

    # mqtt_client.disconnect(client)

    context = {
        "patient": patient,
        "plot_data": plot,
        "sessions": session,
    }
    return HttpResponse(template.render(context, request))



def PatientSession(request, id):
    message = None
    mqtt_client = mqtt.Client()
    patient = Patient.objects.get(id=id)
    template = loader.get_template("patientsession.html")

    # # Get the current time
    # current_time = datetime.now()

    # # Calculate the end time of the countdown (5 minutes from now)
    # end_time = current_time + timedelta(minutes=5)

    # # Calculate the remaining time
    # remaining_time = end_time - current_time

    # # Convert remaining time to seconds
    # remaining_seconds = remaining_time.total_seconds()

    # # Calculate minutes and seconds
    # minutes = int(remaining_seconds // 60)
    # seconds = int(remaining_seconds % 60)
    if patient.session_num == None:
        num = 1
    else:
        num = int(patient.session_num)
        num = num + 1

    patient.session_num = num
    patient.save()

    def on_message(mqtt_client, userdata, msg):

        print(msg.topic+" "+str(msg.payload))
        if msg.payload.decode() == 'End':
            Sessions.objects.create(Patient=patient, session_id = num, session_results = final_data)
            final_data.clear()
        else:
            final_data.append(float(msg.payload.decode()))

    final_data = []
    if request.method == "POST":
        if 'botão_sessão' in request.POST:
            if patient.session_num == None:
                num = 1
            else:
                num = int(patient.session_num)
                num = num + 1

            patient.session_num = num
            patient.save()
            mqtt_client.loop_start()
            mqtt_client.connect(settings.MQTT_SERVER, settings.MQTT_PORT)

            mqtt_client.on_connect = on_connect

            mqtt_client.on_message = on_message
            # Start MQTT client loop (this function will block)


            message = "Session started"

        if 'go_back' in request.POST:
            mqtt_client.loop_stop()
            disconnect(mqtt_client)
            return HttpResponseRedirect('/details/patient/'+str(id))

    context = {
        # "minutes": minutes,
        # "seconds": seconds,
        "patient": patient,
        "message": message,
    }
    return HttpResponse(template.render(context, request))


def SessionReview(request, id, session_id):
    session = Sessions.objects.get(id=session_id)
    patient = Patient.objects.get(id=id)
    template = loader.get_template("sessionreview.html")
    data = session.session_results.strip('[]').split(',')
    data_list = [float(i) for i in data]
    plot = generate_plot(list(np.arange(0,100,(100/len(data_list)))), data_list)

    buf = BytesIO()
    plt.savefig(buf, format='png')
    buf.seek(0)
    plot_data = base64.b64encode(buf.getvalue()).decode('utf-8')

    if request.method == "POST":
        if 'go_back' in request.POST:
            mqtt_client.loop_stop()
            disconnect(mqtt_client)
            return HttpResponseRedirect('/details/patient/'+str(id))

    context = {
        "session": session,
        "patient": patient,
        "plot_data": plot_data,
    }
    return HttpResponse(template.render(context, request))