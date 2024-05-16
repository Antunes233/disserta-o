from django.shortcuts import render, redirect
from django.http import HttpResponse, HttpResponseRedirect
from django.template import loader
from .models import Doctor, Pacient, Sessions
from django.contrib.auth import authenticate, login
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

# Create your views here.
BASE_DIR = settings.BASE_DIR
model_path = os.path.join(BASE_DIR, 'website', 'models_ml', 'KneeFlexionModel.pth')
model = KneeFlexionModel()
model.load_state_dict(torch.load(model_path))


# Initialize MQTT client
mqtt_client = mqtt.Client()



def index(request):
    doc = Doctor.objects.all()
    doc.update()
    return HttpResponse(render(request, "index.html"))


def Details(request, id):
    doc = Doctor.objects.get(id=id)
    template = loader.get_template("details.html")
    pacients = Pacient.objects.filter(doc=doc)
    currentMonth = datetime.now().month
    currentYear = datetime.now().year
    day = datetime.now().day
    calendar = HTMLCalendar().formatmonth(
        currentYear, currentMonth).replace(
            '<td ', '<td  width="50" height="50"')

    context = {
        "mydoc": doc,
        "pacients": pacients,
        "calendar": calendar,

    }
    return HttpResponse(template.render(context, request))


def Login(request):
    if request.method == "POST":
        username = request.POST["username"]
        password = request.POST["password"]

        # Perform manual authentication (you may want to use a more secure method in production)
        try:
            user = Doctor.objects.get(name=username, password=password)
            # Successful login, redirect to a different view or page
            return redirect("details/" + str(user.id))
        except Doctor.DoesNotExist:
            # Invalid credentials, display an error message
            return render(
                request, "loginpage.html", {"error": "Invalid username or password"}
            )

    # Render the login page
    return render(request, "loginpage.html")



def Patientinfo(request, id):
    patient = Pacient.objects.get(id=id)
    template = loader.get_template("patientinfo.html")
    session = Sessions.objects.filter(Pacient_id=id)
    if patient.gender == 'Male':
        gender = 1
    else:
        gender = 0
    with torch.no_grad():
        knee_angle_curve = model(torch.tensor([[patient.age, gender, patient.weight, patient.height]]))
    smoothed_curve = savgol_filter(knee_angle_curve[0].cpu().detach().numpy(), 15, 4)
    plot = generate_plot(list(range(100)), smoothed_curve)

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
    patient = Pacient.objects.get(id=id)
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

    num = int(patient.seesion_num)
    num = num + 1
    patient.seesion_num = num
    patient.save()

    def on_message(mqtt_client, userdata, msg):

        print(msg.topic+" "+str(msg.payload))
        if msg.payload.decode() == 'End':
            Sessions.objects.create(Pacient=patient, session_id = num, session_results = final_data)
            final_data.clear()
        else:
            final_data.append(float(msg.payload.decode()))

    final_data = []
    if request.method == "POST":
        if 'botão_sessão' in request.POST:

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
    patient = Pacient.objects.get(id=id)
    template = loader.get_template("sessionreview.html")

    plot = generate_plot(list(range(199)), list(session.session_results[1:200]))

    buf = BytesIO()
    plt.savefig(buf, format='png')
    buf.seek(0)
    plot_data = base64.b64encode(buf.getvalue()).decode('utf-8')

    context = {
        "session": session,
        "patient": patient,
        "plot_data": plot_data,
    }
    return HttpResponse(template.render(context, request))