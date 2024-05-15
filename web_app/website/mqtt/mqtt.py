import paho.mqtt.client as mqtt
import web_app.settings as settings
from website.models import Pacient, Sessions

class connection_status:
    value = False

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe("django/gait_values")
        connection_status.value = True
    else:
        print("Failed to connect, return code %d\n", rc)

def disconnect(client):
    print("Disconnected from MQTT broker")
    connection_status.value = False
    client.disconnect()


def is_connected():
    return connection_status.value
