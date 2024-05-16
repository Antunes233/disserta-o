import paho.mqtt.client as mqtt
import web_app.settings as settings
from website.models import Pacient, Sessions


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.publish("django/confirm", "Begin Session")
        client.subscribe("django/gait_values")
    else:
        print("Failed to connect, return code %d\n", rc)

def disconnect(client):
    print("Disconnected from MQTT broker")
    client.disconnect()

