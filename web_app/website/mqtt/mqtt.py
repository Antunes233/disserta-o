import paho.mqtt.client as mqtt
import web_app.settings as settings


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe("django/gait_values_r")
        client.subscribe("django/gait_values_l")
    else:
        print("Failed to connect, return code %d\n", rc)

def disconnect(client):
    print("Disconnected from MQTT broker")
    client.disconnect()


